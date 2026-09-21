// CValuePacker packs the values into an ACM-stream.

// IP's packing algorithm is not optimal. For full compatibility uncomment the next line
// #define FULL_IP_COMPAT
// No, do not uncomment. Not implemented yet.

#include <math.h>
#include <string.h>
#include "packer.h"
#include "bitstream.h"
#include "utils.h"

// Two different nearest-roundings. The first is more adequate, but it is slow.
#define ROUND(x) ((int) floor(0.5 + (double)(x)))
//#define ROUND(x) ((int) x)

// returns the approximate packed length of block (based on the max value in the block)
double approx_len (int max, int plus_max) {
	switch (max) {
		case 0: return 0;
		case 1: return 5.0/3;
		case 2: return 7.0/3;
		case 3: return 3.0;
		case 4: if (plus_max <= 3) return 3; else return 7.0/2;
		case 5: return 7.0/2;
		default: plus_max++;
			if (max < plus_max) max = plus_max;
			return (ceil (log ((double) max)/log ((double) 2)) + 1);
	}
}

void CValuePacker::analyse (const int16_t* block) {
	int32_t i, sub_number;
	const int16_t *block_ptr = block;
	int16_t *pblock_ptr = pblock;

	memset (max_abs, 0, sizeof(int16_t)*sb_size);
	memset (max_plus, 0, sizeof(int16_t)*sb_size);

	// 1. transfer the values into internal buffer and locating
	//    max. absolute and max. positive values in the columns
	for (i=0, sub_number=0; i<pblock_size; i++, block_ptr++, pblock_ptr++) {
		int abs_val = *block_ptr;
		*pblock_ptr = (int16_t) abs_val;
		if (abs_val > 0) {
			if (max_plus[sub_number] < abs_val) max_plus[sub_number] = (int16_t) abs_val;
		} else {
			abs_val = -abs_val;
		}
		if (max_abs[sub_number] < abs_val) max_abs[sub_number] = (int16_t) abs_val;
		sub_number++;
		if (sub_number == sb_size) sub_number = 0;
	}

	// 2. try to find the best quantization value:
	//    it is the first value, using which we can fit information into specified bit-limit
	//    We begin from the Greatest Common Divisor (not necessary from 1).
	int val = GCD (pblock, pblock_size); // Greatest Common Divisor
	if (!val) val++; // if GCD is zero, turn it into one

	if (max_bits_limit != -1) {
	// This is not an optimal quantizer in any way (both in the sense of speed and the weighted distortion-rate function).
		// Try to implement bisection alg:
		// 1. looking for an initial range with growing step
		int init_val_was_ok = 1;
		int step = 8;
		while ( estimate (val) > max_bits_limit ) {
			val += (step << 1);
			step <<= 1;
			if (init_val_was_ok) init_val_was_ok = 0;
		}
		// 2. If initial approximation was insufficient, perform the binary search.
		if (!init_val_was_ok) {
			step -= 1;
			val -= step;
			while (step > 0) {
				int half_step = step >> 1;
				if ( estimate (val + half_step) > max_bits_limit ) {
					val += half_step + 1;
					step -= half_step + 1;
				} else
					step = half_step;
			}
		}
	}
	cur_val = (int16_t) val;
	granulate (val);
}

/***************************************************************/
// Faithful replica of the decoder's "juggle" transform (see libacm /
// decoder.cpp), used to predict exactly what the decoder will compute.
static void ck_juggle (int32_t* wrap_p, int32_t* block_p, int sub_len, int sub_count)
{
	int32_t* p;
	int32_t r0, r1, r2, r3;
	for (int i = 0; i < sub_len; i++) {
		p = block_p;
		r0 = wrap_p[0];
		r1 = wrap_p[1];
		for (int j = 0; j < sub_count / 2; j++) {
			r2 = *p;  *p = r1 * 2 + (r0 + r2);  p += sub_len;
			r3 = *p;  *p = r2 * 2 - (r1 + r3);  p += sub_len;
			r0 = r2;  r1 = r3;
		}
		*wrap_p++ = r0;
		*wrap_p++ = r1;
		block_p++;
	}
}

static void ck_juggle_block (int32_t* wrap, int32_t* block, int cols, int rows, int levels)
{
	if (levels == 0)
		return;
	int step_subcount = (levels > 9) ? 1 : ((2048 >> levels) - 2);
	int todo_count = rows;
	int block_off = 0;
	while (1) {
		int sub_count = step_subcount;
		if (sub_count > todo_count)
			sub_count = todo_count;
		int sub_len = cols / 2;
		sub_count *= 2;

		int32_t* wrap_p = wrap;
		int32_t* bp = block + block_off;
		ck_juggle (wrap_p, bp, sub_len, sub_count);
		wrap_p += sub_len * 2;

		for (int i = 0; i < sub_count; i++) {
			bp[0]++;
			bp += sub_len;
		}

		while (sub_len > 1) {
			sub_len /= 2;
			sub_count *= 2;
			ck_juggle (wrap_p, block + block_off, sub_len, sub_count);
			wrap_p += sub_len * 2;
		}
		if (todo_count <= step_subcount)
			break;
		todo_count -= step_subcount;
		block_off += step_subcount << levels;
	}
}

/***************************************************************/
// Transpose (adjoint) of the juggle transform. Running it on an output
// adjoint vector yields the exact sensitivity row: for target output
// position i, A[j] = d(y[i]) / d(coef[j]). This lets decode_check pick
// the single most effective coefficient to adjust, instead of scaling
// the whole block (which stalls when signs cancel at the violator).
// Verified against forward finite differences on 8 shape configs
// (multi-chunk, cross-block) - bit exact.

// Backpropagate through one ck_juggle call.
//   wa: in  = adjoint of this stage's wrap OUTPUTS (2*sub_len entries, the
//             same slots the forward call writes);
//       out (accumulated) = adjoint w.r.t. the wrap INPUTS.
//   A:  in  = adjoint of the stage's block OUTPUTS (sub_len*sub_count ints);
//       out = adjoint w.r.t. the stage's block INPUTS (same positions).
static void ck_juggle_T (int32_t* wa, int32_t* A, int sub_len, int sub_count)
{
	for (int i = 0; i < sub_len; i++) {
		int32_t* a = A + i;
		int32_t q0 = wa[2 * i];
		int32_t q1 = wa[2 * i + 1];
		int32_t t0 = 0, t1 = 0;
		int L = sub_count;
		for (int k = 0; k < L; k++) {
			int32_t g = a[k * sub_len];
			int32_t c;
			if (k == 0) {
				t0 += g;
				t1 += 2 * g;
				c = 1;
			} else if (k == 1) {
				t1 -= g;
				a[0] += 2 * g;
				c = -1;
			} else {
				c = (k & 1) ? -1 : 1;
				a[(k - 1) * sub_len] += 2 * g;
				a[(k - 2) * sub_len] += (k & 1) ? -g : g;
			}
			a[k * sub_len] = c * g;
		}
		a[(L - 2) * sub_len] += q0;
		a[(L - 1) * sub_len] += q1;
		wa[2 * i] += t0;
		wa[2 * i + 1] += t1;
	}
}

// Backpropagate through one ck_juggle_block call (chunks in reverse order).
static void ck_juggle_block_T (int32_t* wrap_adj, int32_t* A, int cols, int rows, int levels)
{
	if (levels == 0)
		return;
	int step_subcount = (levels > 9) ? 1 : ((2048 >> levels) - 2);
	int maxc = rows / step_subcount + 2;
	int* coff = new int [maxc];
	int* crow = new int [maxc];
	int n = 0;
	int todo = rows, off = 0;
	while (1) {
		int rc = step_subcount;
		if (rc > todo) rc = todo;
		coff[n] = off; crow[n] = rc; n++;
		if (todo <= step_subcount) break;
		todo -= step_subcount;
		off += step_subcount << levels;
	}
	int sl[40], soff[40];
	int k = 0, l = cols / 2, acc = 0;
	while (1) {
		sl[k] = l; soff[k] = acc; acc += 2 * l; k++;
		if (l <= 1) break;
		l /= 2;
	}
	int nstages = k;
	for (int c = n - 1; c >= 0; c--) {
		for (int kk = nstages - 1; kk >= 0; kk--) {
			ck_juggle_T (wrap_adj + soff[kk], A + coff[c], sl[kk], crow[c] << (kk + 1));
		}
	}
	delete [] coff;
	delete [] crow;
}

// The decoder computes sample = (juggle_output >> levels) cast to int16.
// The analysis/synthesis pair is not an exact perfect-reconstruction system,
// so for samples at full scale the juggle output may step slightly outside
// the int16 range and the cast wraps around, producing a full-scale click
// (one per occurrence). Prevent this by simulating the decoder on the
// quantized coefficients and repairing violations: using the adjoint of the
// juggle transform, pick the single coefficient with the largest influence
// on the violating output and move it toward zero by the exact needed
// amount (staying inside the block's quantization range, so the already
// written header stays valid). Fallback: a tiny global down-scale. The
// audible impact is nil either way.
void CValuePacker::decode_check ()
{
	if (ck_levels == 0) return;
	const int wrap_cnt = 2 * sb_size - 2;
	const int32_t POS_OK = (int32_t)32768 << ck_levels;  // juggle output must stay below this
	const int32_t NEG_OK = -POS_OK;                      // and at or above this
	int32_t* blk = new int32_t [pblock_size];
	int32_t* wrap = new int32_t [wrap_cnt];
	int32_t* A = new int32_t [pblock_size];
	int32_t* A2 = new int32_t [pblock_size];
	int32_t* WA = new int32_t [wrap_cnt];
	int32_t* blk2 = new int32_t [pblock_size];
	int32_t* wrap2 = new int32_t [wrap_cnt];

	// Scaling target: as the coefficients -> 0, every output moves linearly
	// toward the wrap-only output blk0 = J(ck_wrap, 0), which the previous
	// block's preventive check guaranteed to be inside the range. Hence a
	// global down-scale can never create new violations - it is strictly
	// convergent - while a directed single-coefficient fix can (it may break
	// another output). Strategy: try directed fixes first (minimal damage);
	// if they stall, switch to exact global scaling driven by blk0.
	int32_t* blk0 = new int32_t [pblock_size];
	int32_t* wrap0s = new int32_t [wrap_cnt];
	memset (blk0, 0, sizeof(int32_t) * pblock_size);
	memcpy (wrap0s, ck_wrap, sizeof(int32_t) * wrap_cnt);
	ck_juggle_block (wrap0s, blk0, sb_size, subblocks, ck_levels);
	// wrap0s = wrap state after this block if its coefficients were zero
	int64_t best_worst = -1;
	int stall = 0;
	bool global_only = false;
	bool tgt2_ready = false;
	for (int iter = 0; iter < 300; iter++) {
		// decoded output of this block
		for (int i = 0; i < pblock_size; i++)
			blk[i] = (int32_t) pblock[i] * (int32_t) cur_val;
		memcpy (wrap, ck_wrap, sizeof(int32_t) * wrap_cnt);
		ck_juggle_block (wrap, blk, sb_size, subblocks, ck_levels);

		// find the worst violation; 'need' = signed amount to add to the output
		int pos = -1, through_next = 0;
		int64_t worst_mag = 0, need = 0;
		for (int i = 0; i < pblock_size; i++) {
			int64_t over;
			if (blk[i] >= POS_OK) over = (int64_t) blk[i] - (POS_OK - 1);
			else if (blk[i] < NEG_OK) over = (int64_t) NEG_OK - blk[i];
			else continue;
			if (over > worst_mag) {
				worst_mag = over; pos = i;
				need = (blk[i] >= POS_OK) ? (int64_t)(POS_OK - 1) - blk[i]
				                          : (int64_t) NEG_OK - blk[i];
			}
		}
		if (pos < 0) {
			// preventive check: the wrap state this block leaves behind feeds
			// the start of the next block. Even if the next block is silent,
			// the ringing must stay inside int16, otherwise that block could
			// click and no repair of it alone could fully fix it.
			memcpy (wrap2, wrap, sizeof(int32_t) * wrap_cnt);
			memset (blk2, 0, sizeof(int32_t) * pblock_size);
			ck_juggle_block (wrap2, blk2, sb_size, subblocks, ck_levels);
			for (int i = 0; i < pblock_size; i++) {
				int64_t over;
				if (blk2[i] >= POS_OK) over = (int64_t) blk2[i] - (POS_OK - 1);
				else if (blk2[i] < NEG_OK) over = (int64_t) NEG_OK - blk2[i];
				else continue;
				if (over > worst_mag) {
					worst_mag = over; pos = i; through_next = 1;
					need = (blk2[i] >= POS_OK) ? (int64_t)(POS_OK - 1) - blk2[i]
					                          : (int64_t) NEG_OK - blk2[i];
				}
			}
			if (pos < 0)
				break; // block and its aftermath are clean
		}

		// stall tracking: if the worst violation stops improving, stop trying
		// directed fixes (they can fight each other) and scale globally.
		if (!global_only) {
			if (best_worst < 0 || worst_mag < best_worst) {
				best_worst = worst_mag;
				stall = 0;
			} else if (++stall > 20) {
				global_only = true;
			}
		}

		if (!global_only) {
			// sensitivity row of the violating output w.r.t. the coefficients
			memset (A, 0, sizeof(int32_t) * pblock_size);
			memset (WA, 0, sizeof(int32_t) * wrap_cnt);
			if (!through_next) {
				A[pos] = 1;
				ck_juggle_block_T (WA, A, sb_size, subblocks, ck_levels);
			} else {
				// violation lives in a hypothetical silent next block: backprop
				// through that block, then through this block's wrap outputs
				memset (A2, 0, sizeof(int32_t) * pblock_size);
				A2[pos] = 1;
				ck_juggle_block_T (WA, A2, sb_size, subblocks, ck_levels);
				memset (A, 0, sizeof(int32_t) * pblock_size);
				ck_juggle_block_T (WA, A, sb_size, subblocks, ck_levels);
			}

			// pick the single most effective coefficient. The decoder rebuilds
			// coefficients from an amplitude table holding units in
			// [-2^pwr, 2^pwr-1] (pwr = this block's header field); the table
			// itself is int16, so unit*val must also stay inside int16 or the
			// decoder silently wraps it. Both toward-zero and away-from-zero
			// moves are legal while the unit stays inside these bounds.
			const int32_t UP_LIM = (int32_t) 1 << ck_pwr;      // amp-table |unit| bound
			int32_t vpos = (cur_val > 0) ? cur_val : 1;
			int32_t P_MAX = ((UP_LIM - 1) < 32767 / vpos) ? (UP_LIM - 1) : 32767 / vpos; // max positive unit
			int32_t N_MIN = (-(int32_t) UP_LIM > -(32768 / vpos)) ? -(int32_t) UP_LIM : -(32768 / vpos); // min negative unit
			int64_t best_p = 0, best_cap = 0;
			int best_j = -1, best_s = 0;
			for (int j = 0; j < pblock_size; j++) {
				if (A[j] == 0) continue;
				int32_t pj = pblock[j];
				int64_t u = (int64_t) cur_val * (int64_t) A[j];
				// distance to the opposite bound (do not overshoot into it)
				int64_t room = (need < 0) ? (int64_t) blk[pos] - NEG_OK
				                          : (int64_t)(POS_OK - 1) - blk[pos];
				if (pj == 0) {
					// only an away move (to +1 or -1) can affect the output
					if (P_MAX < 1 && N_MIN > -1) continue;
					int64_t p = u, q = -u; // progress per +1 / per -1 unit
					int32_t s = 0; int64_t pp = 0;
					if (p != 0 && ((need < 0) == (p < 0))) { s = 1; pp = p; }
					else if (q != 0 && ((need < 0) == (q < 0))) { s = -1; pp = q; }
					else continue;
					int64_t ap = (pp < 0) ? -pp : pp;
					int64_t cap = (s > 0) ? (int64_t) P_MAX : -(int64_t) N_MIN;
					int64_t cap2 = room / ap;
					if (cap2 < cap) cap = cap2;
					if (cap >= 1 && ap > best_p) { best_p = ap; best_cap = cap; best_j = j; best_s = s; }
					continue;
				}
				int32_t sgn = (pj > 0) ? 1 : -1;
				// option 1: toward zero (always within the bounds)
				{
					int64_t p = -(int64_t) sgn * u;
					if (p != 0 && ((need < 0) == (p < 0))) {
						int64_t ap = (p < 0) ? -p : p;
						int64_t cap = (pj > 0) ? (int64_t) pj : -(int64_t) pj;
						int64_t cap2 = room / ap;
						if (cap2 < cap) cap = cap2;
						if (cap >= 1 && ap > best_p) { best_p = ap; best_cap = cap; best_j = j; best_s = -sgn; }
					}
				}
				// option 2: away from zero, up to the amp-table/int16 bounds
				{
					int64_t p = (int64_t) sgn * u;
					if (p != 0 && ((need < 0) == (p < 0))) {
						int64_t ap = (p < 0) ? -p : p;
						int64_t cap = (pj > 0) ? (int64_t) P_MAX - pj
						                       : (int64_t) pj - N_MIN;
						int64_t cap2 = room / ap;
						if (cap2 < cap) cap = cap2;
						if (cap >= 1 && ap > best_p) { best_p = ap; best_cap = cap; best_j = j; best_s = sgn; }
					}
				}
			}
			if (best_j >= 0) {
				int64_t aneed = (need < 0) ? -need : need;
				int64_t delta = (aneed + best_p - 1) / best_p; // ceil
				if (delta > best_cap) delta = best_cap;
				pblock[best_j] = (int16_t) (pblock[best_j] + best_s * (int32_t) delta);
				continue; // recompute with the repaired coefficient
			}
			global_only = true; // no single-coefficient move helps
		}

		// global down-scale with an exact factor: y(f) moves linearly from the
		// current output toward the in-range target, so pick f that just brings
		// the worst violator into range; f2 guarantees rounding makes progress.
		int32_t* act = through_next ? blk2 : blk;    // f = 1 outputs
		int32_t* tgt = through_next ? NULL : blk0;   // f = 0 outputs
		if (through_next) {
			// target = silent-next output when this block's coefficients are 0:
			// juggle(wrap0s, 0); computed once, cached in blk2/wrap2 slots later
			if (!tgt2_ready) {
				memcpy (wrap2, wrap0s, sizeof(int32_t) * wrap_cnt);
				memset (A2, 0, sizeof(int32_t) * pblock_size);
				ck_juggle_block (wrap2, A2, sb_size, subblocks, ck_levels);
				tgt2_ready = true;
			}
			tgt = A2; // reuse A2 as the cached target outputs
		}
		double fstar = 1.0;
		for (int i = 0; i < pblock_size; i++) {
			if (act[i] < POS_OK && act[i] >= NEG_OK) continue;
			double den = (double) act[i] - (double) tgt[i];
			if (den == 0) continue;
			double num = (act[i] >= POS_OK) ? (double)(POS_OK - 1) - (double) tgt[i]
			                                : (double) NEG_OK - (double) tgt[i];
			double fi = num / den;
			if (fi < fstar) fstar = fi;
		}
		int32_t nmax = 1;
		for (int i = 0; i < pblock_size; i++) {
			int32_t a = pblock[i] >= 0 ? (int32_t) pblock[i] : -(int32_t) pblock[i];
			if (a > nmax) nmax = a;
		}
		if (nmax <= 1)
			break; // nothing left to reduce
		double f2 = (double)(nmax - 1) / (double) nmax;
		double f = (fstar < f2) ? fstar : f2;
		if (f < 0.0) f = 0.0;
		if (f > 1.0) f = 1.0;
		for (int i = 0; i < pblock_size; i++) {
			double n = f * (double) pblock[i];
			pblock[i] = (int16_t) floor (0.5 + n);
		}
	}
	// commit the wrap state produced by the coefficients actually kept
	for (int i = 0; i < pblock_size; i++)
		blk[i] = (int32_t) pblock[i] * (int32_t) cur_val;
	memcpy (wrap, ck_wrap, sizeof(int32_t) * wrap_cnt);
	ck_juggle_block (wrap, blk, sb_size, subblocks, ck_levels);
	memcpy (ck_wrap, wrap, sizeof(int32_t) * wrap_cnt);
	delete [] blk;
	delete [] wrap;
	delete [] A;
	delete [] A2;
	delete [] WA;
	delete [] blk2;
	delete [] wrap2;
	delete [] blk0;
	delete [] wrap0s;
}

double CValuePacker::estimate (int val) {
	double res = 0;
	for (int i=0; i<sb_size; i++)
		res += approx_len (ROUND(max_abs[i]/val), ROUND(max_plus[i]/val));
	res *= subblocks;
// Headers are not taken into consideration at the moment.
// No, they are! Because when sb_size is large, they cannot be ignored.
// No, they are not!
//	res += 20 + 5*sb_size; // size of headers
	return res;
}

void CValuePacker::granulate (int val) {
	int max = 0; // the maximum amplitude
	for (int i=0; i<pblock_size; i++) {
		int n = ROUND (pblock[i] / val); // "degranulate"
		pblock[i] = (int16_t) n;
		n = (n<0)? -n: n+1;
		if (n > max) max = n;
	}

	int pwr = (int)ceil (log ((double) max)/log ((double) 2));
	// pwr must be at least 1: the decoder's amplitude table is built with
	// 1<<pwr positive entries, and the +/-1 codebooks (k13/k12/t15/...)
	// index buff_middle[1]. With pwr=0 that entry is never initialized and
	// a "+1" coefficient decodes as heap garbage (audible click).
	if (pwr < 1) pwr = 1;
#ifdef FULL_IP_COMPAT
// In Interplay's ACMs the pwr is not less than 3:
	if (pwr < 3) pwr = 3;
#endif
	ck_pwr = pwr;

	bit_stream->write_bits (pwr, 4);
	bit_stream->write_bits (val, 16);
}

enum k_enum {K13, K12, K24, K23, K35, K34, K45, K44};

void CValuePacker::pack_column (int col) {
	int p0 = 0, p_all_0 = 0, p00 = 0, p1 = 0;  // count of single zero, all zeros, pairs of zeros, and ones
	int max_amp = 0;  // max absolute amplitude
	int max_plus_amp = 0;
	int p00_x3, pall0_x3; // triple p00 and p_all_0 (user very frequently)
	int i;
	int16_t* current = pblock + col;

	// 1. collecting statistics
	for (i=0; i<subblocks; i++, current += sb_size) {
		if (*current == 0) {
			p_all_0++; // some zero
			if (current[sb_size] == 0) {
				p00++ ; // doubled zero
				i++;
				current += sb_size; // skip the next item
				if (i<subblocks) p_all_0++; // but remeber, it was a zero
			} else
				p0++; // single zero
		} else {
			int abs_val = *current;
			if (abs_val > 0) {
				if (max_plus_amp < abs_val) max_plus_amp = abs_val;
			} else {
				abs_val = -abs_val;
			}
			if (max_amp < abs_val) max_amp = abs_val;
			if (abs_val == 1) p1++; // one more one
		}
	}

	// 2. thinking about gained results
	p00_x3 = p00 * 3;
	pall0_x3 = p_all_0 * 3;
	switch (max_amp) {
		case 0: bit_stream->write_bits (0, 5); break;// ZeroFill
		case 1:
			if (p00_x3 > subblocks) 	make_k (K13, col);
			else if (pall0_x3 > subblocks)	make_k (K12, col);
			else                            make_t15 (col);
			break;
		case 2:
			if (p00_x3 > subblocks) 	make_k (K24, col);
			else if (pall0_x3 > subblocks)	make_k (K23, col);
			else                            make_t27 (col);
			break;
		case 3:
			if (p00_x3 > subblocks) 	make_k (K35, col);
			else if (pall0_x3 + p1 > subblocks) make_k (K34, col);
			else                            make_linear (3, col);
			break;
		case 4:
		if (max_plus_amp <= 3) {
			if (p00_x3 > subblocks) 	make_k (K45, col);
			else if (pall0_x3 > subblocks)	make_k (K44, col);
			else                            make_linear (3, col);
			break;
		} else {
			if (p00_x3 > subblocks) 	make_k (K45, col);
			else if (2*pall0_x3 > subblocks) make_k (K44, col);
			else                            make_t37 (col);
		}
			break;
		case 5:  make_t37 (col); break;
		default:
			max_plus_amp++;
			if (max_amp < max_plus_amp) max_amp = max_plus_amp;
			int pwr = (int)ceil (log ((double) max_amp)/log ((double) 2));
			make_linear (pwr+1, col);
	}
}

void CValuePacker::add_one_block (const int16_t* block) {
	analyse (block);
	decode_check ();
	for (int i=0; i<sb_size; i++)
		pack_column (i);
}

int CValuePacker::init_packer() {
	pblock = new int16_t [pblock_size + 2*sb_size]; // two more lines (are always zero)
	if (!pblock) return 0;
	bit_stream = new CBitStream (file); if ( !bit_stream || !bit_stream->init_bit_stream() ) return 0;
	for (int i=0; i<2*sb_size; i++) pblock[pblock_size + i] = 0;
	max_abs = new int16_t [sb_size]; if (!max_abs) return 0;
	max_plus = new int16_t [sb_size]; if (!max_plus) return 0;
	ck_wrap = new int32_t [2 * sb_size - 2]; if (!ck_wrap) return 0;
	memset (ck_wrap, 0, sizeof(int32_t) * (2 * sb_size - 2));
	ck_levels = 0;
	while ((1 << ck_levels) < sb_size) ck_levels++;
	return 1;
}

int32_t CValuePacker::flush_bit_stream() {
	bit_stream->flush();
	return ( bit_stream->get_bytes_written() );
}

struct one_val { char bits; char val; };
struct maker_desc {
	char number;
	int double_zero;
	char base;
	one_val* data;
};
// Values:         -4     -3     -2      -1       0      1       2       3       4
one_val k13v[] = {                      {3,3},  {2,1}, {3,7}                         },
        k12v[] = {                      {2,1},  {1,0}, {2,3}                         },
        k24v[] = {              {4,3},  {4,7},  {2,1}, {4,11}, {4,15}                },
        k23v[] = {              {3,1},  {3,3},  {1,0}, {3,5},  {3,7}                 },
        k35v[] = {       {5,7}, {5,15}, {4,3},  {2,1}, {4,11}, {5,23}, {5,31}        },
        k34v[] = {       {4,3}, {4,7},  {3,1},  {1,0}, {3,5},  {4,11}, {4,15}        },
        k45v[] = {{5,3}, {5,7}, {5,11}, {5,15}, {2,1}, {5,19}, {5,23}, {5,27}, {5,31}},
        k44v[] = {{4,1}, {4,3}, {4,5},  {4,7},  {1,0}, {4,9},  {4,11}, {4,13}, {4,15}};
maker_desc k_desc[] = {
	{17, 1, 1, k13v}, {18, 0, 1, k12v},
	{20, 1, 2, k24v}, {21, 0, 2, k23v},
	{23, 1, 3, k35v}, {24, 0, 3, k34v},
	{26, 1, 4, k45v}, {27, 0, 4, k44v}
};

void CValuePacker::make_k (int ind, int col) {
	int double_zero = k_desc[ind].double_zero;
	char base = k_desc[ind].base;
	one_val* data = k_desc[ind].data;
	int16_t* curr = pblock + col;

	bit_stream->write_bits (k_desc[ind].number, 5);
	for (int i=0; i<subblocks; i++, curr += sb_size) {
		if (double_zero && (curr[0] == 0) && (curr[sb_size] == 0)) {
			bit_stream->write_bits (0, 1);
			i++; curr += sb_size;
		} else {
			one_val item = data[base + *curr];
			bit_stream->write_bits (item.val, item.bits);
		}
	}
}

void CValuePacker::make_linear (int bits, int col) {
	int16_t base = (int16_t) (1 << (bits-1));
	int16_t* curr = pblock + col;

	bit_stream->write_bits (bits, 5);
	for (int i=0; i<subblocks; i++, curr += sb_size) {
		bit_stream->write_bits (base + *curr, bits);
	}
}

void CValuePacker::make_t15 (int col) {
	int16_t *curr = pblock + col;
	int step = sb_size*3;

	bit_stream->write_bits (19, 5);
	for (int i=0; i<subblocks; i+=3, curr+=step) {
		int val = (1 + *curr) + (1 + curr[sb_size])*3 + (1 + curr[2*sb_size])*9;
		bit_stream->write_bits (val, 5);
	}
}

void CValuePacker::make_t27 (int col) {
	int16_t *curr = pblock + col;
	int step = sb_size*3;

	bit_stream->write_bits (22, 5);
	for (int i=0; i<subblocks; i+=3, curr+=step) {
		int val = (2 + *curr) + (2 + curr[sb_size])*5 + (2 + curr[2*sb_size])*25;
		bit_stream->write_bits (val, 7);
	}
}

void CValuePacker::make_t37 (int col) {
	int16_t *curr = pblock + col;
	bit_stream->write_bits (29, 5);
	for (int i=0; i<subblocks; i+=2, curr+=2*sb_size) {
		int val = (5 + *curr) + (5 + curr[sb_size])*11;
		bit_stream->write_bits (val, 7);
	}
}
