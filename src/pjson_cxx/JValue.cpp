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

#include <JValue.h>

#include <pbnjson.h>
#include <pbnjson_experimental.h>
#include <JSchema.h>
#include <JGenerator.h>
#include <cassert>

#ifdef DBG_CXX_MEM_STR
#define PJ_DBG_CXX_STR(expr) expr
#else
#define PJ_DBG_CXX_STR(expr) do { } while(0)
#endif

namespace pbnjson {

JValue JValue::JNULL(jnull());

static inline raw_buffer strToRawBuffer(const std::string& str)
{
	return (raw_buffer){str.c_str(), str.length()};
}

JValue::JValue()
#ifdef _DEBUG
	: m_jval(jvalue_copy(JNULL.m_jval))
#else
	: m_jval(JNULL.m_jval)
#endif
{
}

JValue::JValue(jvalue_ref toOwn)
	: m_jval(toOwn)
{
	if (toOwn == NULL)
		m_jval = JNULL.m_jval;
}

JValue::JValue(jvalue_ref parsed, std::string input)
	: m_jval(parsed), m_input(input)
{
	// if this assertion doesn't hole the optimization parameters in parse are
	// invalid.
	assert(input.c_str() == m_input.c_str());
	PJ_DBG_CXX_STR(std::cerr << "Have handle to string at " << (void*)m_input.c_str() << std::endl);
}

template <>
JValue::JValue(const int32_t& value)
	: m_jval(jnumber_create_i64(value))
{
}

template <>
JValue::JValue(const int64_t& value)
	: m_jval(jnumber_create_i64(value))
{
}

template <>
JValue::JValue(const double& value)
	: m_jval(jnumber_create_f64(value))
{
}

template <>
JValue::JValue(const std::string &value)
	: m_input(value)
{
	PJ_DBG_CXX_STR(std::cerr << "Have handle to string at " << (void*)m_input.c_str() << std::endl);
	assert(m_input.c_str() == value.c_str());
#if PBNJSON_ZERO_COPY_STL_STR
	m_jval = jstring_create_nocopy(strToRawBuffer(m_input));
	assert(jstring_get_fast(m_jval).m_str == m_input.c_str());
	assert(jstring_get_fast(m_jval).m_len == m_input.length());
#else
	m_jval = jstring_create_utf8(m_input.c_str(), m_input.size());
#endif
}

JValue::JValue(const char *str)
	: m_input(str)
{
	PJ_DBG_CXX_STR(std::cerr << "Have handle to string at " << (void*)m_input.c_str() << std::endl);
#if PBNJSON_ZERO_COPY_STL_STR
	m_jval = jstring_create_nocopy(strToRawBuffer(m_input));
	assert(jstring_get_fast(m_jval).m_str == m_input.c_str() || m_input.length() == 0);
	assert(jstring_get_fast(m_jval).m_len == m_input.length());
#else
	m_jval = jstring_create_utf8(m_input.c_str(), m_input.size());
#endif
}

template <>
JValue::JValue(const bool& value)
	: m_jval(jboolean_create(value))
{
}

template<>
JValue::JValue(const NumericString& value)
	: m_input(value)
{
#if PBNJSON_ZERO_COPY_STL_STR
	m_jval = jnumber_create_unsafe(strToRawBuffer(m_input), NULL);

#ifdef _DEBUG
	{
		raw_buffer result;
		jnumber_get_raw(m_jval, &result);
		assert(m_input.c_str() == result.m_str);
	}
#endif
#else
	m_jval = jnumber_create(strToRawBuffer(value));
#endif
}

JValue::~JValue()
{
	PJ_DBG_CXX_STR(std::cerr << "Releasing handle to " << (void *)m_input.c_str() << std::endl);
	j_release(&m_jval);
}

JValue::JValue(const JValue& other)
	: m_jval(jvalue_copy(other.m_jval)), m_input(other.m_input)
#if PBNJSON_ZERO_COPY_STL_STR
	, m_children(other.m_children)
#endif
{
}

JValue& JValue::operator=(const JValue& other)
{
	if (m_jval != other.m_jval) {
		j_release(&m_jval);
		m_jval = jvalue_copy(other.m_jval);
		m_input = other.m_input;
#if PBNJSON_ZERO_COPY_STL_STR
		m_children = other.m_children;
#endif
	}
	return *this;
}

JValue Object()
{
	return jobject_create();
}

JValue Array()
{
	return jarray_create(NULL);
}

#if 0
template <>
JValue JValue::Value<int64_t>(const int64_t& value)
{
	return JValue(value);
}

template <>
JValue JValue::Value<double>(const double& value)
{
	return jnumber_create_f64(value);
}

template <>
JValue JValue::Value<std::string>(const std::string &value)
{
	// already have the length - why not use it instead of calling strlen one more time
	return JValue (jstring_create_nocopy(strToRawBuffer(value)), value);
}

template<>
JValue JValue::Value<NumericString>(const NumericString& value)
{
	return JValue (jnumber_create_unsafe(strToRawBuffer(static_cast<std::string>(value)), NULL), value);
}

template <>
JValue JValue::Value<bool>(const bool& value)
{
	return jboolean_create(value);
}
#endif

bool JValue::operator==(const JValue& other) const
{
	if (this == &other)
		return true;
	if (m_jval == other.m_jval)
		return true;
	assert(!isNull() || !other.isNull());
	if (isNull() || other.isNull())
		return false;

	if (isObject()) {
		if (!other.isObject())
			return false;
		jobject_key_value value;
		jvalue_ref tmpVal;
		for (jobject_iter i = jobj_iter_init(m_jval); jobj_iter_is_valid(i); i = jobj_iter_next(i)) {
			if (jobj_iter_deref(i, &value)) {
				if (!jobject_get_exists(other.m_jval, jstring_get_fast(value.key), &tmpVal))
					return false;
				if (JValue(value.value) != JValue(tmpVal))
					return false;
			} else {
				assert(false);
			}
			return false;
		}
	} else if (isArray()) {
		if (!other.isArray())
			return false;
		if (jarray_size(m_jval) != jarray_size(other.m_jval))
			return false;
		for (ssize_t i = jarray_size(m_jval) - 1; i >= 0; i--) {
			if (JValue(jarray_get(m_jval, i)) != JValue(jarray_get(other.m_jval, i)))
				return false;
		}
	} else if (isString()) {
		if (!other.isString())
			return false;
		return jstring_equal(m_jval, other.m_jval);
	} else if (isNumber()) {
		if (!other.isNumber())
			return false;
		double myNumber, otherNumber;

		ConversionResultFlags myError = asNumber(myNumber);
		ConversionResultFlags otherError = asNumber(otherNumber);

		return myError == otherError && myNumber == otherNumber;
	}
	ConversionResultFlags isBool, otherIsBool;
	bool myVal, otherVal;
	isBool = asBool(myVal);
	otherIsBool = asBool(otherVal);

	return isBool == CONV_OK && otherIsBool == CONV_OK && myVal == otherVal;
}

template <class T>
static bool numEqual(const JValue& jnum, const T& nativeNum)
{
	T num;
	if (jnum.asNumber(num) == CONV_OK)
		return num == nativeNum;
	return false;
}

bool JValue::operator==(const std::string& other) const
{
	std::string strRep;
	if (asString(strRep) == CONV_OK)
		return strRep == other;
	return false;
}

bool JValue::operator==(const double& other) const
{
	return numEqual(*this, other);
}

bool JValue::operator==(const int64_t& other) const
{
	return numEqual(*this, other);
}

bool JValue::operator==(int32_t other) const
{
	return numEqual(*this, other);
}

bool JValue::operator==(bool other) const
{
	bool value;
	if (asBool(value) == CONV_OK)
		return value == other;
	return false;
}


JValue JValue::operator[](int index) const
{
	return jvalue_copy(jarray_get(m_jval, index));
}

JValue JValue::operator[](const std::string& key) const
{
	return this->operator[](j_str_to_buffer(key.c_str(), key.size()));
}

JValue JValue::operator[](const raw_buffer& key) const
{
	return jvalue_copy(jobject_get(m_jval, key));
}

bool JValue::put(size_t index, const JValue& value)
{
#if PBNJSON_ZERO_COPY_STL_STR
	m_children.push_back(value.m_input);
	m_children.insert(m_children.end(), value.m_children.begin(), value.m_children.end());
#endif
	return jarray_put(m_jval, index, jvalue_copy(value.peekRaw()));
}

bool JValue::put(const std::string& key, const JValue& value)
{
	return put(JValue(key), value);
}

bool JValue::put(const JValue& key, const JValue& value)
{
#if PBNJSON_ZERO_COPY_STL_STR
	m_children.push_back(value.m_input);
	m_children.push_back(key.m_input);
	m_children.insert(m_children.end(), value.m_children.begin(), value.m_children.end());
#endif
	return jobject_put(m_jval, jvalue_copy(key.peekRaw()), jvalue_copy(value.peekRaw()));
}

JValue& JValue::operator<<(const JValue& element)
{
	if (!append(element))
		return Null();
	return *this;
}

JValue& JValue::operator<<(const KeyValue& pair)
{
	if (!put(pair.first, pair.second))
		return Null();
	return *this;
}


bool JValue::append(const JValue& value)
{
#if PBNJSON_ZERO_COPY_STL_STR
	if (!value.m_input.empty() || (value.isString() && value.asString().empty())) {
		m_children.push_back(value.m_input);
	}
	m_children.insert(m_children.end(), value.m_children.begin(), value.m_children.end());
#endif
	return jarray_append(m_jval, jvalue_copy(value.peekRaw()));
}

bool JValue::hasKey(const std::string& key) const
{
	return jobject_get_exists(m_jval, strToRawBuffer(key), NULL);
}

ssize_t JValue::arraySize() const
{
	return jarray_size(m_jval);
}

bool JValue::isNull() const
{
	return jis_null(m_jval);
}

bool JValue::isNumber() const
{
	return jis_number(m_jval);
}

bool JValue::isString() const
{
	return jis_string(m_jval);
}

bool JValue::isObject() const
{
	return jis_object(m_jval);
}

bool JValue::isArray() const
{
	return jis_array(m_jval);
}

bool JValue::isBoolean() const
{
	return jis_boolean(m_jval);
}

template <>
ConversionResultFlags JValue::asNumber<int32_t>(int32_t& number) const
{
	return jnumber_get_i32(m_jval, &number);
}

template <>
ConversionResultFlags JValue::asNumber<int64_t>(int64_t& number) const
{
	return jnumber_get_i64(m_jval, &number);
}

template <>
ConversionResultFlags JValue::asNumber<double>(double& number) const
{
	return jnumber_get_f64(m_jval, &number);
}

template <>
ConversionResultFlags JValue::asNumber<std::string>(std::string& number) const
{
	raw_buffer asRaw;
	ConversionResultFlags result;

	result = jnumber_get_raw(m_jval, &asRaw);
	number = std::string(asRaw.m_str, asRaw.m_len);

	return result;
}

template <>
ConversionResultFlags JValue::asNumber<NumericString>(NumericString& number) const
{
	std::string num;
	ConversionResultFlags result;

	result = asNumber(num);
	number = num;

	return result;
}

template <>
int32_t JValue::asNumber<int32_t>() const
{
	int32_t result;
	asNumber(result);
	return result;
}

template <>
int64_t JValue::asNumber<int64_t>() const
{
	int64_t result;
	asNumber(result);
	return result;
}

template <>
double JValue::asNumber<double>() const
{
	double result;
	asNumber(result);
	return result;
}

template <>
std::string JValue::asNumber<std::string>() const
{
	std::string result;
	asNumber(result);
	return result;
}

template <>
NumericString JValue::asNumber<NumericString>() const
{
	return NumericString(asNumber<std::string>());
}

ConversionResultFlags JValue::asString(std::string &asStr) const
{
	if (!isString()) {
		return CONV_NOT_A_STRING;
	}

	raw_buffer backingBuffer = jstring_get_fast(m_jval);
	if (backingBuffer.m_str == NULL) {
		asStr = "";
		return CONV_NOT_A_STRING;
	}

	asStr = std::string(backingBuffer.m_str, backingBuffer.m_len);

	return CONV_OK;
}

#if 0
bool JValue::toString(const JSchema& schema, std::string &toStr) const
{
	JGenerator generator;
	return generator.toString(*this, schema, toStr);
}
#endif

ConversionResultFlags JValue::asBool(bool &result) const
{
	return jboolean_get(m_jval, &result);
}

JValue::ObjectIterator::ObjectIterator()
	: i(jobj_iter_init(NULL)), m_parent(NULL)
{
}

JValue::ObjectIterator::ObjectIterator(jvalue_ref parent, const jobject_iter& other)
	: i(other), m_parent(jvalue_copy(parent))
{
}

JValue::ObjectIterator::ObjectIterator(const ObjectIterator& other)
	: i(other.i), m_parent(jvalue_copy(other.m_parent))
{
}

JValue::ObjectIterator::~ObjectIterator()
{
	j_release(&m_parent);
}

JValue::ObjectIterator& JValue::ObjectIterator::operator=(const ObjectIterator &other)
{
	if (this != &other) {
		i.m_opaque = other.i.m_opaque;
		j_release(&m_parent);
		m_parent = jvalue_copy(other.m_parent);
	}
	return *this;
}

/**
 * specification says it's undefined, but implementation-wise,
 * the C api will return the current iterator if you try to go past the end.
 *
 */
JValue::ObjectIterator& JValue::ObjectIterator::operator++()
{
	i = jobj_iter_next(i);
	return *this;
}

JValue::ObjectIterator JValue::ObjectIterator::operator++(int)
{
	ObjectIterator result(*this);
	++(*this);
	return result;
}

JValue::ObjectIterator JValue::ObjectIterator::operator+(int n) const
{
	ObjectIterator next(m_parent, i);
	for (int j = n; j > 0; j--)
		++next;
	return next;
}

JValue::ObjectIterator& JValue::ObjectIterator::operator--()
{
	i = jobj_iter_previous(i);
	return *this;
}

JValue::ObjectIterator JValue::ObjectIterator::operator--(int)
{
	ObjectIterator result(*this);
	--(*this);
	return result;
}

JValue::ObjectIterator JValue::ObjectIterator::operator-(int n) const
{
	ObjectIterator previous(*this);
	for (int j = n; j > 0; j--)
		--previous;
	return previous;
}

bool JValue::ObjectIterator::operator==(const ObjectIterator& other) const
{
	return this == &other || jobj_iter_equal(i, other.i);
}

JValue::KeyValue JValue::ObjectIterator::operator*()
{
	KeyValue m_keyval;
	jobject_key_value pair;
	if (jobj_iter_deref(i, &pair)) {
		m_keyval = KeyValue(jvalue_copy(pair.key), jvalue_copy(pair.value));
	}
	else
		m_keyval = KeyValue(JValue::Null(), JValue::Null());
	return m_keyval;
}

JValue::ObjectConstIterator::ObjectConstIterator()
	: i(jobj_iter_init(NULL)), m_parent(NULL)
{
}

JValue::ObjectConstIterator::ObjectConstIterator(jvalue_ref parent, const jobject_iter& other)
	: i(other), m_parent(jvalue_copy(parent))
{
}

JValue::ObjectConstIterator::ObjectConstIterator(const ObjectConstIterator& other)
	: i(other.i), m_parent(jvalue_copy(other.m_parent))
{
}

JValue::ObjectConstIterator::~ObjectConstIterator()
{
	j_release(&m_parent);
}

JValue::ObjectConstIterator& JValue::ObjectConstIterator::operator=(const ObjectConstIterator &other)
{
	if (this != &other) {
		i.m_opaque = other.i.m_opaque;
		j_release(&m_parent);
		m_parent = jvalue_copy(other.m_parent);
	}
	return *this;
}

/**
 * specification says it's undefined, but implementation-wise,
 * the C api will return the current iterator if you try to go past the end.
 *
 */
JValue::ObjectConstIterator& JValue::ObjectConstIterator::operator++()
{
	i = jobj_iter_next(i);
	return *this;
}

JValue::ObjectConstIterator JValue::ObjectConstIterator::operator++(int)
{
	ObjectConstIterator result(*this);
	++(*this);
	return result;
}

JValue::ObjectConstIterator JValue::ObjectConstIterator::operator+(int n) const
{
	ObjectConstIterator next(m_parent, i);
	for (int j = n; j > 0; j--)
		++next;
	return next;
}

JValue::ObjectConstIterator& JValue::ObjectConstIterator::operator--()
{
	i = jobj_iter_previous(i);
	return *this;
}

JValue::ObjectConstIterator JValue::ObjectConstIterator::operator--(int)
{
	ObjectConstIterator result(*this);
	--(*this);
	return result;
}

JValue::ObjectConstIterator JValue::ObjectConstIterator::operator-(int n) const
{
	ObjectConstIterator previous(*this);
	for (int j = n; j > 0; j--)
		--previous;
	return previous;
}

bool JValue::ObjectConstIterator::operator==(const ObjectConstIterator& other) const
{
	return this == &other || jobj_iter_equal(i, other.i);
}

#if 0
JValue::KeyValue JValue::ObjectConstIterator::operator*()
{
	KeyValue m_keyval;
	jobject_key_value pair;
	if (jobj_iter_deref(i, &pair))
		m_keyval = KeyValue(jvalue_copy(pair.key), jvalue_copy(pair.value));
	else
		m_keyval = KeyValue(JValue::Null(), JValue::Null());
	return m_keyval;
}
#endif

/**
 * specification says it's undefined. in the current implementation
 * though, jobj_iter_init should return end() when this isn't an object
 * (it also takes care of printing errors to the log)
 */
JValue::ObjectIterator JValue::begin()
{
	jobject_iter i = jobj_iter_init(m_jval);
	return ObjectIterator(m_jval, i);
}

/**
 * Specification says it's undefined.  In the current implementation
 * though, jobj_iter_init_last will return a NULL pointer when this isn't
 * an object (it also takes care of printing errors to the log)
 *
 * Specification says undefined if we try to iterate - current implementation
 * won't let you iterate once you hit end.
 */
JValue::ObjectIterator JValue::end()
{
	jobject_iter i = jobj_iter_init_last(m_jval);
	return ObjectIterator(m_jval, i);
}

/**
 * specification says it's undefined. in the current implementation
 * though, jobj_iter_init should return end() when this isn't an object
 * (it also takes care of printing errors to the log)
 */
JValue::ObjectConstIterator JValue::begin() const
{
	jobject_iter i = jobj_iter_init(m_jval);
	return ObjectConstIterator(m_jval, i);
}

/**
 * Specification says it's undefined.  In the current implementation
 * though, jobj_iter_init_last will return a NULL pointer when this isn't
 * an object (it also takes care of printing errors to the log)
 *
 * Specification says undefined if we try to iterate - current implementation
 * won't let you iterate once you hit end.
 */
JValue::ObjectConstIterator JValue::end() const
{
	jobject_iter i = jobj_iter_init_last(m_jval);
	return ObjectConstIterator(m_jval, i);
}

NumericString::operator JValue()
{
	return JValue(*this);
}

}
