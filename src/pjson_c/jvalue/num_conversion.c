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

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <assert.h>
#include <inttypes.h>

#include <jtypes.h>

#include "num_conversion.h"
#include <compiler/nonnull_attribute.h>
#include <compiler/pure_attribute.h>
#include <compiler/builtins.h>
#include "../liblog.h"

#define PJSON_MAX_INT INT32_MAX
#define PJSON_MIN_INT INT32_MIN

#define PJSON_MAX_INT64 INT64_MAX
#define PJSON_MIN_INT64 INT64_MIN

#define PJSON_MAX_INT_IN_DBL INT64_C(0x1FFFFFFFFFFFFF)
#define PJSON_MIN_INT_IN_DBL -PJSON_MAX_INT_IN_DBL

#ifdef PJSON_SAFE_NOOP_CONVERSION
static ConversionResult ji32_noop(int32_t value, int32_t *result)
{
	CHECK_POINTER_RETURN_VALUE(result, CONV_BAD_ARGS);
	*result = value;
	return CONV_OK;
}

static ConversionResult ji64_noop(int64_t value, int64_t *result)
{
	CHECK_POINTER_RETURN_VALUE(result, CONV_BAD_ARGS);
	*result = value;
	return CONV_OK;
}

static ConversionResult jdouble_noop(double value, double *result)
{
	CHECK_POINTER_RETURN_VALUE(result, CONV_BAD_ARGS);
	*result = value;
	return CONV_OK;
}
#else
#define ji32_noop NULL
#define ji64_noop NULL
#define jdouble_noop NULL
#endif /* PJSON_SAFE_NOOP_CONVERSION */

ConversionResultFlags parseJSONNumber(raw_buffer *str, int64_t *integerPortion,
		int64_t *exponentPortion, int64_t *decimalPortion, int64_t *decimalLeadingZeros)
{
	size_t i = 0;
	int integerMultiplier = 1;
	int exponentMultiplier = 1;
	int64_t exponent = 0;
	int64_t fraction = 0;
	int64_t fractionFactor = 0;
	int64_t trailingZeros = 0;
	bool validDecimal;
	int64_t temp;

	ConversionResultFlags result = CONV_OK;

	*integerPortion = 0;

	if (str->m_len == 0) {
		result = CONV_NOT_A_NUM;
		goto fast_stop;
	}

	switch (str->m_str[i]) {
		case '-':
			integerMultiplier = -1;
			i++;
			goto parse_integer_portion;
		case '0':
			i++;
			if (i >= str->m_len)
				goto fast_stop;
			switch(str->m_str[i])
			{
			case '.':
				goto parse_decimal_portion;
			case 'e':
			case 'E':
				goto parse_exponent_portion;
			default:
				result = CONV_NOT_A_NUM;
				goto fast_stop;
			}
		case '1'...'9':
			goto parse_integer_portion;
		default:
			goto not_a_number;
	}

parse_integer_portion:
	assert(integerMultiplier == 1 || integerMultiplier == -1);
	assert(exponent == 0);
	for (; i  < str->m_len; i++) {
		switch (str->m_str[i]) {
			case 'e':
			case 'E':
				goto parse_exponent_portion;
			case '.':
				goto parse_decimal_portion;
			case '0'...'9':
				if (exponent == 0) {
					if (integerMultiplier == 1) {
						if (*integerPortion > (INT64_MAX / 10))
							exponent = 1;
					} else {
						if (*integerPortion < (INT64_MIN) / 10)
							exponent = 1;
					}
					if (exponent == 0) {
						temp = *integerPortion * 10 + integerMultiplier * (str->m_str[i] - '0');
						if (UNLIKELY(temp * integerMultiplier < 0)) {
							// sign flipped - overflow
							exponent = 1;
						} else {
							*integerPortion = temp;
						}
					}
				} else {
					if (exponent++ == INT64_MAX)
						return CONV_PRECISION_LOSS | (integerMultiplier == 1 ? CONV_POSITIVE_INFINITY : CONV_NEGATIVE_INFINITY);
				}
				break;
			default:
				PJ_LOG_WARN("Unexpected character %d('%c') in '%.*s' at %zu", (int)str->m_str[i], str->m_str[i], (int)str->m_len, str->m_str, i);
				goto not_a_number;
		}
	}
	goto finish_parse;

parse_decimal_portion:
	validDecimal = false;
	assert(fraction == 0);
	assert(fractionFactor == 0);
	assert(trailingZeros == 0);

	if (str->m_str[i] != '.') {
		assert(false);
		PJ_LOG_WARN("Unexpected character %d('%c') in '%.*s' at %zu", (int)str->m_str[i], str->m_str[i], (int) str->m_len, str->m_str, i);
		goto not_a_number;
	}
	i++;

	for (; i < str->m_len; i++) {
		switch(str->m_str[i]) {
			case 'e':
			case 'E':
				goto parse_exponent_portion;
			case '0'...'9':
				validDecimal = true;
				if (str->m_str[i] == '0') {
					// short-circuit - trailing 0s are ignored if that's what they are.
					trailingZeros ++;
					break;
				}
				if (UNLIKELY(fractionFactor == INT64_MAX)) {
					assert(false);
					// this will only become an issue if 10^INT64_MAX < (2^((sizeof(fraction)*8) - 1) - 1)
					// which will never happen
					PJ_LOG_ERR("Internal error for input: %.*s", (int)str->m_len, str->m_str);
					return CONV_GENERIC_ERROR;
				}

				while (trailingZeros != 0) {
					temp = fraction * 10;
					if (temp < 0)
						goto skip_remaining_decimal;
					trailingZeros--;
					fractionFactor++;
					fraction = temp;
				}
				fractionFactor++;

				if (fraction != INT64_MAX) {
					temp = fraction * 10 + (str->m_str[i] - '0');
					if (UNLIKELY(temp < 0)) {
						fractionFactor--;
						goto skip_remaining_decimal;
					} else {
						fraction = temp;
					}
				}
				break;
			default:
				PJ_LOG_WARN("Unexpected character %d('%c') in '%.*s' at %zu", (int)str->m_str[i], str->m_str[i], (int)str->m_len, str->m_str, i);
				goto not_a_number;
		}
	}
	if (UNLIKELY(!validDecimal)) {
		PJ_LOG_WARN("Unexpected end of string at %zu in '%.*s'", i, (int)str->m_len, str->m_str);
		goto not_a_number;
	}
	goto finish_parse;

skip_remaining_decimal:
	assert(str->m_str[i] >= '0');
	assert(str->m_str[i] <= '9');

	result |= CONV_PRECISION_LOSS;

	for (; i < str->m_len; i++) {
		if (str->m_str[i] >= '0' && str->m_str[i] <= '9')
			continue;
		if (str->m_str[i] == 'e' || str->m_str[i] == 'E')
			goto parse_exponent_portion;

		PJ_LOG_WARN("Unexpected character %d('%c') in '%.*s' at %zu", (int)str->m_str[i], str->m_str[i], (int)str->m_len, str->m_str, i);
		goto not_a_number;
	}
	assert(i == str->m_len);
	goto finish_parse;

parse_exponent_portion:
	assert(exponent >= 0);
	if (UNLIKELY(str->m_str[i] != 'e' && str->m_str[i] != 'E')) {
		// problem with the state machine
		assert(false);
		PJ_LOG_ERR("Expecting an exponent but didn't get one at %zu in '%.*s'", i, (int)str->m_len, str->m_str);
		return CONV_GENERIC_ERROR;
	}
	i++;

	switch (str->m_str[i]) {
	case '-':
		i++;
		exponentMultiplier = -1;
		break;
	case '+':
		i++;
	case '0'...'9':
		exponentMultiplier = 1;
		break;
	default:
		PJ_LOG_WARN("Unexpected character %d('%c') in '%.*s' at %zu", (int)str->m_str[i], str->m_str[i], (int)str->m_len, str->m_str, i);
		goto not_a_number;
	}
	assert(exponentMultiplier == 1 || exponentMultiplier == -1);

	for (; i < str->m_len; i++) {
		switch (str->m_str[i]) {
			case '0'...'9':
				if (exponentMultiplier == 1) {
					if (UNLIKELY(exponent > (INT64_MAX / 10)))
						goto exponent_overflow;
				} else if (exponentMultiplier == -1) {
					if (UNLIKELY(exponent < (INT64_MIN / 10)))
						goto exponent_overflow;
				}
				exponent *= 10;
				exponent += exponentMultiplier * (str->m_str[i] - '0');
				if (exponent * exponentMultiplier < 0) {
					goto exponent_overflow;
				}
				break;
			default:
				PJ_LOG_WARN("Unexpected character %d('%c') in '%.*s' at %zu", (int)str->m_str[i], str->m_str[i], (int)str->m_len, str->m_str, i);
				goto not_a_number;
		}
	}
	assert(i == str->m_len);
	goto finish_parse;

exponent_overflow:
	// overflow of a 64-bit exponent - +/- infinity or 0 it is.
	assert(exponent > (INT64_MAX / 10 - 10) || exponent < (INT64_MIN / 10 + 10));

	if (exponentMultiplier == 1) {
		exponent = INT64_MAX;
		if (integerMultiplier == 1) {
			*integerPortion = INT64_MAX;
			result |= CONV_POSITIVE_INFINITY;
		} else {
			*integerPortion = INT64_MIN;
			result |= CONV_NEGATIVE_INFINITY;
		}
	} else {
		result |= CONV_PRECISION_LOSS;
		exponent = INT64_MIN;
		*integerPortion = 0;
	}
	goto finish_parse;

finish_parse:
	if (trailingZeros) {
		PJ_LOG_INFO("%"PRId64 " unnecessary 0s in fraction portion of '%.*s'", trailingZeros, (int)str->m_len, str->m_str);
	}

	if (fraction == 0) {
		assert(fractionFactor == 0);
	}

	if (*integerPortion == 0 && (decimalPortion == NULL || fraction == 0)) {
		// shortcut - exponent is redundant if the number is 0.something but we're
		// ignoring the decimal (or there's no fractional portion)
		exponent = 0;
		if (fraction != 0) {
			result |= CONV_PRECISION_LOSS;
		}
	}

	// can't really do this anyways - it would require us shifting values into or out
	// of the fractional component when we adjust the integerPortion by the exponent.
	// internally, we would never use this case anyways because if we care what the
	// fraction is (i.e. we're converting to a floating point), we'll provide the exponent
	// pointer anyways
	if (exponentPortion == NULL && exponent != 0 && fraction != 0) {
		result |= CONV_PRECISION_LOSS;
		fraction = 0;
		fractionFactor = 0;
	}

	if (!exponentPortion) {
		if (*integerPortion != 0) {
			if (exponent > 0) {
				while (exponent) {
					if (*integerPortion > INT64_MAX / 10) {
						assert(integerMultiplier == 1);
						result |= CONV_POSITIVE_OVERFLOW;
						*integerPortion = INT64_MAX;
						break;
					} else if (*integerPortion < INT64_MIN / 10) {
						assert(integerMultiplier == -1);
						result |= CONV_NEGATIVE_OVERFLOW;
						*integerPortion = INT64_MIN;
						break;
					}
					if (*integerPortion != 0)
						assert(*integerPortion * 10 > 0);
					*integerPortion *= 10;
					exponent--;
				}
			} else if (exponent < 0) {
				if (fraction) {
					result |= CONV_PRECISION_LOSS;
					goto lost_precision;
				}
				while (exponent) {
					if (*integerPortion % 10 != 0) {
						result |= CONV_PRECISION_LOSS;
						goto lost_precision;
					}
					*integerPortion /= 10;
					exponent++;
				}
lost_precision:
				while (exponent++ && *integerPortion > 0)
					*integerPortion /= 10;
			}
		}
	} else {
		*exponentPortion = exponent;
	}

	if (!decimalPortion) {
		if (fraction != 0) {
			result |= CONV_PRECISION_LOSS;
		}
	} else {
		*decimalPortion = fraction;
		*decimalLeadingZeros = fractionFactor;
	}

	return result;

not_a_number:
	return CONV_NOT_A_NUM;

fast_stop:
	if (exponentPortion) *exponentPortion = exponent;
	if (decimalPortion) *decimalPortion = fraction;
	if (decimalLeadingZeros) *decimalLeadingZeros = fractionFactor;
	return result;
}

/**
 * Solve the equation "number E x"
 *
 * @param number The number to multiply by a factor of 10
 * @param x The exponent to take the 10 to.
 *
 * @return number * (10^x)
 */
static double expBase10(int64_t number, int64_t x) PURE_FUNC;
static double expBase10(int64_t number, int64_t x)
{
	double result = number;
	while (x > 0 && !isinf(result) && !isinf(-result)) {
		result *= 10;
		x--;
	}
	while (x < 0 && result != 0) {
		result /= 10;
		x++;
	}
	return result;
}

ConversionResultFlags jstr_to_i32(raw_buffer *str, int32_t *result)
{
	ConversionResultFlags status1, status2 = CONV_GENERIC_ERROR;
	int64_t bigResult = 0;
	status1 = jstr_to_i64(str, &bigResult);
	if (LIKELY(status1 == CONV_OK))
		status2 = ji64_to_i32(bigResult, result);
	return status1 != CONV_OK ? status1 : status2;
}

ConversionResultFlags jstr_to_i64(raw_buffer *str, int64_t *result)
{
	ConversionResultFlags conv_result;

	CHECK_POINTER_RETURN_VALUE(str->m_str, CONV_BAD_ARGS);
	CHECK_POINTER_RETURN_VALUE(result, CONV_BAD_ARGS);

	conv_result = parseJSONNumber(str, result, NULL, NULL, NULL);
	if (CONV_HAS_POSITIVE_OVERFLOW(conv_result))
		*result = INT64_MAX;
	else if (CONV_HAS_NEGATIVE_OVERFLOW(conv_result))
		*result = INT64_MIN;
	return conv_result;
}

ConversionResultFlags jstr_to_double(raw_buffer *str, double *result)
{
	ConversionResultFlags conv_result;

	int64_t wholeComponent = 0;
	int64_t fraction = 0;
	int64_t fractionLeadingZeros = 0;
	int64_t exponent = 0;

	CHECK_POINTER_RETURN_VALUE(result, CONV_BAD_ARGS);

	conv_result = parseJSONNumber(str, &wholeComponent, &exponent, &fraction, &fractionLeadingZeros);

	if (UNLIKELY(CONV_IS_BAD_ARGS(conv_result) || CONV_IS_GENERIC_ERROR(conv_result))) {
		PJ_LOG_ERR("Some weird problem converting %.*s to a number: %x", (int)str->m_len, str->m_str, conv_result);
		assert(false);
		*result = DBL_QUIET_NAN;
	} else if (UNLIKELY(CONV_HAS_POSITIVE_INFINITY(conv_result))) {
		*result = HUGE_VAL;
	} else if (UNLIKELY(CONV_HAS_NEGATIVE_INFINITY(conv_result))) {
		*result = -HUGE_VAL;
	} else {
		if (CONV_HAS_OVERFLOW(conv_result)) {
			// overflow that isn't infinity is precision loss
			assert (CONV_HAS_PRECISION_LOSS(conv_result));
		}

		double calculatedWhole = expBase10(wholeComponent, exponent);
		double calculatedFraction = copysign(expBase10(fraction, exponent - fractionLeadingZeros), calculatedWhole);
		*result = calculatedWhole + calculatedFraction;
		if (isinf(*result))
			conv_result |= CONV_POSITIVE_OVERFLOW;
		else if (-isinf(*result))
			conv_result |= CONV_NEGATIVE_OVERFLOW;
		else if (*result == 0 && fraction != 0)
			conv_result |= CONV_PRECISION_LOSS;
	}

	return conv_result;
}

ConversionResultFlags jdouble_to_i32(double value, int32_t *result)
{
	CHECK_POINTER_RETURN_VALUE(result, CONV_BAD_ARGS);
	
	if (isnan(value) != 0) {
		PJ_LOG_WARN("attempting to convert nan to int");
		*result = 0;
		return CONV_NOT_A_NUM;
	}

	switch (isinf(value)) {
		case 0:
			break;
		case 1:
			PJ_LOG_WARN("attempting to convert +infinity to int");
			*result = PJSON_MAX_INT;
			return CONV_POSITIVE_INFINITY;
		case -1:
			PJ_LOG_WARN("attempting to convert -infinity to int");
			*result = PJSON_MIN_INT;
			return CONV_NEGATIVE_INFINITY;
		default:
			PJ_LOG_ERR("unknown result from isinf for %lf", value);
			return CONV_GENERIC_ERROR;
	}

	if (value > PJSON_MAX_INT) {
		PJ_LOG_WARN("attempting to convert double %lf outside of int range", value);
		*result = PJSON_MAX_INT;
		return CONV_POSITIVE_OVERFLOW;
	}

	if (value < PJSON_MIN_INT) {
		PJ_LOG_WARN("attempting to convert double %lf outside of int range", value);
		*result = PJSON_MIN_INT;
		return CONV_NEGATIVE_OVERFLOW;
	}

#if 0
	// unnecessary for 32-bits because they will always fit in a double
	// with no precision loss
	if (value > PJSON_MAX_INT_IN_DBL || value < PJSON_MIN_INT_IN_DBL) {
		PJ_LOG_INFO("conversion of double %lf to integer potentially has precision loss");
		*result = (int64_t)value;
		return CONV_PRECISION_LOSS;
	}
#endif

	*result = (int32_t) value;
	if (*result != value) {
		PJ_LOG_INFO("conversion of double %lf results in integer with different value", value);
		return CONV_PRECISION_LOSS;
	}

	return CONV_OK;
}

ConversionResultFlags jdouble_to_i64(double value, int64_t *result)
{
	CHECK_POINTER_RETURN_VALUE(result, CONV_BAD_ARGS);
	
	if (isnan(value) != 0) {
		PJ_LOG_WARN("attempting to convert nan to int64");
		*result = 0;
		return CONV_NOT_A_NUM;
	}

	switch (isinf(value)) {
		case 0:
			break;
		case 1:
			PJ_LOG_WARN("attempting to convert +infinity to int");
			*result = PJSON_MAX_INT64;
			return CONV_POSITIVE_INFINITY;
		case -1:
			PJ_LOG_WARN("attempting to convert -infinity to int");
			*result = PJSON_MIN_INT64;
			return CONV_NEGATIVE_INFINITY;
		default:
			PJ_LOG_ERR("unknown result from isinf for %lf", value);
			return CONV_GENERIC_ERROR;
	}

	if (value > PJSON_MAX_INT64) {
		PJ_LOG_WARN("attempting to convert double %lf outside of int64 range", value);
		*result = PJSON_MAX_INT64;
		return CONV_POSITIVE_OVERFLOW;
	}

	if (value < PJSON_MIN_INT64) {
		PJ_LOG_WARN("attempting to convert double %lf outside of int64 range", value);
		*result = PJSON_MIN_INT64;
		return CONV_NEGATIVE_OVERFLOW;
	}

	if (value > PJSON_MAX_INT_IN_DBL || value < PJSON_MIN_INT_IN_DBL) {
		PJ_LOG_INFO("conversion of double %lf to integer potentially has precision loss", value);
		*result = (int64_t)value;
		return CONV_PRECISION_LOSS;
	}

	*result = (int64_t) value;
	if (*result != value) {
		PJ_LOG_INFO("conversion of double %lf results in integer with different value", value);
		return CONV_PRECISION_LOSS;
	}
	return CONV_OK;
}

ConversionResultFlags ji32_to_i64(int32_t value, int64_t *result)
{
	CHECK_POINTER_RETURN_VALUE(result, CONV_BAD_ARGS);
	*result = value;
	return CONV_OK;
}

ConversionResultFlags ji32_to_double(int32_t value, double *result)
{
	CHECK_POINTER_RETURN_VALUE(result, CONV_BAD_ARGS);
	*result = value;
	return CONV_OK;
}

ConversionResultFlags ji64_to_i32(int64_t value, int32_t *result)
{
	if (value > PJSON_MAX_INT) {
		PJ_LOG_WARN("overflow converting %"PRId64 " to int32", value);
		*result = PJSON_MAX_INT;
		return CONV_POSITIVE_OVERFLOW;
	}
	if (value < PJSON_MIN_INT) {
		PJ_LOG_WARN("overflow converting %"PRId64 " to int32", value);
		*result = PJSON_MIN_INT;
		return CONV_NEGATIVE_OVERFLOW;
	}
	*result = (int32_t) value;
	return CONV_OK;
}

ConversionResultFlags ji64_to_double(int64_t value, double *result)
{
	CHECK_POINTER_RETURN_VALUE(result, CONV_BAD_ARGS);
	if (value > PJSON_MAX_INT_IN_DBL || value < PJSON_MIN_INT_IN_DBL) {
		PJ_LOG_INFO("conversion of integer %"PRId64 " to a double will result in precision loss when doing reverse", value);
		*result = (double)value;
		return CONV_PRECISION_LOSS;
	}
	*result = (double)value;
	return CONV_OK;
}

#if 0
static ConversionMatrix __conversion = {
	{ jstr_to_i32, ji32_noop, ji64_to_i32, jdouble_to_i32 },
	{ jstr_to_i64, ji32_to_i64, ji64_noop, jdouble_to_i64 },
	{ jstr_to_double, ji32_to_double, ji64_to_double, jdouble_noop }
};

ConversionMatrix* NUMERIC_CONVERSION = &__conversion;
#endif

