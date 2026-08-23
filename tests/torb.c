/*
    Orb edge-quality probe.

    Includes orb.c directly to reach the
    static per-line renderer, then prints
    the alpha ramp across the boundary of
    the middle row.
*/
#include "../orb.c"

#include <stdio.h>

int main(void)
{
    Uint32 pixels[ORB_RES * ORB_RES];

    memset(pixels, 0, sizeof(pixels));

    int mid = ORB_RES / 2;

    render_orb_line(
        mid, pixels, ORB_RES, ORB_RES,
        1.0f, 0.5f);

    /*
        Walk the row; report every alpha
        transition steeper than 96 per texel
        (i.e. harder than ~2.6 texels full
        ramp) inside the edge zone.
    */
    int prev = -1;
    int hard = 0;
    int first = -1, last = -1;

    for (int x = 0; x < ORB_RES; x++) {

        int a =
            (pixels[mid * ORB_RES + x]
                >> 24) & 0xFF;

        if (a > 0 && first < 0)
            first = x;

        if (a > 0)
            last = x;

        if (prev >= 0 &&
            prev - a > 96) {
            printf("HARD DROP %d -> %d "
                   "at x=%d\n",
                   prev, a, x);
            hard++;
        }

        prev = a;
    }

    /*
        Show the actual ramp for eyeballing.
    */
    printf("ramp: ");

    for (int x = first > 0 ? first - 2 : 0;
         x <= last + 2 && x < ORB_RES;
         x++) {

        int a =
            (pixels[mid * ORB_RES + x]
                >> 24) & 0xFF;

        printf("%d ", a / 26);
    }

    printf("\n");

    if (hard) {
        printf("FAIL: %d hard edges\n",
               hard);
        return 1;
    }

    printf("PASS: boundary is smooth\n");
    return 0;
}
