

/*
 *  This Quickselect routine is based on the algorithm described in
 *  "Numerical recipes in C", Second Edition,
 *  Cambridge University Press, 1992, Section 8.5, ISBN 0-521-43108-5
 *  This code by Nicolas Devillard - 1998. Public domain.
 */


#ifndef __CUSTOM_QUICKSELECT_H__
#define __CUSTOM_QUICKSELECT_H__

#include <gst/gst.h>
#include <stdint.h>

uint8_t quick_select_uint8_t(uint8_t arr[], int n);

uint16_t quick_select_uint16_t(uint16_t arr[], int n);

#endif