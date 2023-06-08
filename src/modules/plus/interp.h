//interp.c
/*
 * Copyright (C) 2010 Marko Cebokli   http://lea.hamradio.si/~s57uuu
 * Copyright (C) 2010-2023 Meltytech, LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <inttypes.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

//#define TEST_XY_LIMITS

//--------------------------------------------------------
// pointer to an interpolating function
// parameters:
//  source image
//  source width
//  source height
//  X coordinate
//  Y coordinate
//  opacity
//  destination image
//  flag to overwrite alpha channel

typedef int (*interpp)(unsigned char *, int, int, float, float, float, unsigned char *, int);

// nearest neighbor

int interpNN_b32(
    unsigned char *s, int w, int h, float x, float y, float o, unsigned char *d, int is_atop)
{
#ifdef TEST_XY_LIMITS
    if (x < 0 || x >= w || y < 0 || y >= h)
        return -1;
#endif
    int p = 4 * ((int) x + (int) y * w);
    float alpha_s = s[p + 3] / 255.0f * o;
    float alpha_d = d[3] / 255.0f;
    float alpha = alpha_s + alpha_d - alpha_s * alpha_d;
    d[3] = is_atop ? s[p + 3] : (255 * alpha);
    alpha = alpha_s / alpha;
    d[0] = d[0] * (1.0f - alpha) + s[p] * alpha;
    d[1] = d[1] * (1.0f - alpha) + s[p + 1] * alpha;
    d[2] = d[2] * (1.0f - alpha) + s[p + 2] * alpha;

    return 0;
}

//  bilinear

int interpBL_b32(
    unsigned char *s, int w, int h, float x, float y, float o, unsigned char *d, int is_atop)
{
#ifdef TEST_XY_LIMITS
    if (x < 0 || x >= w || y < 0 || y >= h)
        return -1;
#endif

    int m = (int) (x + 0.5f) - 1;
    int n = (int) (y + 0.5f) - 1;
    unsigned char S[16];
    if (m >= 0 && n >= 0) {
        if (m + 1 < w && n + 1 < h) {
            int k = 4 * (m + w * n);
            memcpy(S, s + k, 8);
            memcpy(S + 8, s + (k + 4 * w), 8);
        }
        else if (n + 1 < h) {
            int k = w - 1 + w * n;
            *(int *) S = ((int *) s)[k];
            ((int *) S)[1] = *(int *) S;
            ((int *) S)[2] = ((int *) s)[k + w];
            ((int *) S)[3] = ((int *) S)[2];
        }
        else if (m + 1 < w) {
            memcpy(S, s + 4 * (m + w * (h - 1)), 8);
            memcpy(S + 8, S, 8);
        }
        else {
            int a = ((int *) s)[w * h - 1];
            *(int *) S = a;
            ((int *) S)[1] = a;
            ((int *) S)[2] = a;
            ((int *) S)[3] = a;
        }
    }
    else if (n >= 0) {
        if (n + 1 < h) {
            *(int *) S = ((int *) s)[w * n];
            ((int *) S)[1] = *(int *) S;
            ((int *) S)[2] = ((int *) s)[w * (n + 1)];
            ((int *) S)[3] = ((int *) S)[2];
        }
        else {
            int a = ((int *) s)[w * (h - 1)];
            *(int *) S = a;
            ((int *) S)[1] = a;
            ((int *) S)[2] = a;
            ((int *) S)[3] = a;
        }
    }
    else if (m >= 0) {
        if (m + 1 < w) {
            memcpy(S, s + 4 * m, 8);
            memcpy(S + 8, S, 8);
        }
        else {
            int a = ((int *) s)[w - 1];
            *(int *) S = a;
            ((int *) S)[1] = a;
            ((int *) S)[2] = a;
            ((int *) S)[3] = a;
        }
    }
    else {
        int a = *(int *) s;
        *(int *) S = a;
        ((int *) S)[1] = a;
        ((int *) S)[2] = a;
        ((int *) S)[3] = a;
    }

    float dx = x - 0.5f - m;
    float dy = y - 0.5f - n;

    float a = S[3] + (S[7] - S[3]) * dx;
    float b = S[11] + (S[15] - S[11]) * dx;

    float alpha_s = a + (b - a) * dy;
    float alpha_d = d[3] / 255.0f;
    if (is_atop)
        d[3] = alpha_s;
    alpha_s = alpha_s / 255.0f * o;
    float alpha = alpha_s + alpha_d - alpha_s * alpha_d;
    if (!is_atop)
        d[3] = 255 * alpha;
    alpha = alpha_s / alpha;

    a = *S + (S[4] - *S) * dx;
    b = S[8] + (S[12] - S[8]) * dx;
    *d = *d * (1.0f - alpha) + (a + (b - a) * dy) * alpha;

    a = S[1] + (S[5] - S[1]) * dx;
    b = S[9] + (S[13] - S[9]) * dx;
    d[1] = d[1] * (1.0f - alpha) + (a + (b - a) * dy) * alpha;

    a = S[2] + (S[6] - S[2]) * dx;
    b = S[10] + (S[14] - S[10]) * dx;
    d[2] = d[2] * (1.0f - alpha) + (a + (b - a) * dy) * alpha;

    return 0;
}

// bicubic

int interpBC_b32(
    unsigned char *s, int w, int h, float x, float y, float o, unsigned char *d, int is_atop)
{
    int i, j, b, l, m, n;
    float k;
    float p[4], p1[4], p2[4], p3[4], p4[4];
    float alpha = 1.0;

#ifdef TEST_XY_LIMITS
    if ((x < 0) || (x >= w) || (y < 0) || (y >= h))
        return -1;
#endif

    m = (int) ceilf(x) - 2;
    if (m < 0)
        m = 0;
    if ((m + 5) > w)
        m = w - 4;
    n = (int) ceilf(y) - 2;
    if (n < 0)
        n = 0;
    if ((n + 5) > h)
        n = h - 4;

    for (b = 3; b > -1; b--) {
        // first after y (four columns)
        for (i = 0; i < 4; i++) {
            l = m + (i + n) * w;
            p1[i] = s[4 * l + b];
            p2[i] = s[4 * (l + 1) + b];
            p3[i] = s[4 * (l + 2) + b];
            p4[i] = s[4 * (l + 3) + b];
        }
        for (j = 1; j < 4; j++)
            for (i = 3; i >= j; i--) {
                k = (y - i - n) / j;
                p1[i] = p1[i] + k * (p1[i] - p1[i - 1]);
                p2[i] = p2[i] + k * (p2[i] - p2[i - 1]);
                p3[i] = p3[i] + k * (p3[i] - p3[i - 1]);
                p4[i] = p4[i] + k * (p4[i] - p4[i - 1]);
            }

        // now after x
        p[0] = p1[3];
        p[1] = p2[3];
        p[2] = p3[3];
        p[3] = p4[3];
        for (j = 1; j < 4; j++)
            for (i = 3; i >= j; i--)
                p[i] = p[i] + (x - i - m) / j * (p[i] - p[i - 1]);

        if (p[3] < 0.0f)
            p[3] = 0.0f;
        if (p[3] > 255.0f)
            p[3] = 255.0f;

        if (b == 3) {
            float alpha_s = (float) p[3] / 255.0f * o;
            float alpha_d = (float) d[3] / 255.0f;
            alpha = alpha_s + alpha_d - alpha_s * alpha_d;
            d[3] = is_atop ? p[3] : (255 * alpha);
            alpha = alpha_s / alpha;
        } else {
            d[b] = d[b] * (1.0f - alpha) + p[3] * alpha;
        }
    }

    return 0;
}
