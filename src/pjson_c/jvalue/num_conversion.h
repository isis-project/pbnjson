/* @@@LICENSE
*
*      Copyright (c) 2012 Hewlett-Packard Development Company, L.P.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
LICENSE@@@ */

#ifndef JNUM_CONVERSION_INTERNAL_H_
#define JNUM_CONVERSION_INTERNAL_H_

#include <stdint.h>
#include <jconversion.h>
#include <japi.h>
#include <stdlib.h>
#include <compiler/nonnull_attribute.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Breaks down a JSON numeric-string into its components.
 *
 * The number can be calculated as integerPortion * 10^exponent + decimalPortion * 10^(exponent-decimalLeadingZeros) .
 *
 * NOTE: Behaviour is unspecified if exponent isn't NULL but decimalPortion & decimalLeadingZeros are.
 *
 * @param string
 * @param strlen
 * @param integerPortion - Cannot be null.  Will contain the integer equivalent of the number (clamped to the boundaries)
 * @param exponent - If not null, then the exponent is not applied.  If it is, an attempt to apply the exponent is made
 *                   (if there is overflow, the result flags are
 * @param decimalPortion - If null & there is a decimal point, then a CONV_PRECISION_LOSS will be present in the result flag.  Otherwise,
 *                         this will contain the numeric equivalent of the numbers following the decimal point
 * @param decimalLeadingZeros - Cannot be null if decimalPortion isn't null.  This will contain the number of leading 0s.  In other words,
 *                              decimalPortion * 10^(exponent - decimalLeadingZeros) gives the fractional part of the number.  If exponent is NULL,
 *                              then an attempt is made to incorporate it into decimalLeadingZeros
 *                              NOTE: Behaviour is undefined if this is NULL but decimalPortion isn't
 * @return Any conversion errors.  Best effort made to catch all errors, but no guarantees are made about how a failure to convert will fail.
 *         CONV_NOT_A_NUM, CONV_GENERIC_ERROR, & CONV_BAD_ARGS are mutually exclusive values from any other flags (== can be used).
 */
PJSON_LOCAL ConversionResultFlags parseJSONNumber(raw_buffer *str, int64_t *integerPortion,
		int64_t *exponentPortion, int64_t *decimalPortion, int64_t *decimalLeadingZeros) NON_NULL(1, 2);

PJSON_LOCAL ConversionResultFlags jstr_to_i32(raw_buffer *str, int32_t *result);
PJSON_LOCAL ConversionResultFlags jstr_to_i64(raw_buffer *str, int64_t *result);
PJSON_LOCAL ConversionResultFlags jstr_to_double(raw_buffer *str, double *result);
PJSON_LOCAL ConversionResultFlags jdouble_to_i32(double value, int32_t *result);
PJSON_LOCAL ConversionResultFlags jdouble_to_i64(double value, int64_t *result);
PJSON_LOCAL ConversionResultFlags jdouble_to_str(double value, raw_buffer *str);
PJSON_LOCAL ConversionResultFlags ji32_to_i64(int32_t value, int64_t *result);
PJSON_LOCAL ConversionResultFlags ji32_to_double(int32_t value, double *result);
PJSON_LOCAL ConversionResultFlags ji32_to_str(int32_t value, raw_buffer *str);
PJSON_LOCAL ConversionResultFlags ji64_to_i32(int64_t value, int32_t *result);
PJSON_LOCAL ConversionResultFlags ji64_to_double(int64_t value, double *result);
PJSON_LOCAL ConversionResultFlags ji64_to_str(int64_t value, raw_buffer *str);

#if 0
typedef ConversionResultFlags (*string_conversion)(const char *string, size_t strLen, void *result);
typedef ConversionResultFlags (*int32_conversion)(int32_t value, void *result);
typedef ConversionResultFlags (*int64_conversion)(int64_t value, void *result);
typedef ConversionResultFlags (*double_conversion)(double value, void *result);

typedef struct {
	string_conversion from_str;
	int32_conversion from_int32;
	int64_conversion from_int64;
	double_conversion from_double;
} PJSON_LOCAL ConversionRow;

typedef struct {
	ConversionRow to_int32;
	ConversionRow to_int64;
	ConversionRow to_double;
	ConversionRow to_string;
} PJSON_LOCAL ConversionMatrix;

extern ConversionMatrix* NUMERIC_CONVERSION;
#endif

#ifdef __cplusplus
}
#endif

#endif /* JNUM_CONVERSION_INTERNAL_H_ */
