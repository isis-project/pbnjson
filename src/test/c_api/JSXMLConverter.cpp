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

#include "JSXMLConverter.h"
#include <QDomDocument>
#include <QXmlAttributes>
#include <QMap>
#include <QByteArray>
#include <QtDebug>
#include <pbnjson.h>

bool operator<(const JValueWrapper &v1w, const JValueWrapper &v2w)
{
	jvalue_ref v1 = (jvalue_ref)v1w;
	jvalue_ref v2 = (jvalue_ref)v2w;
	raw_buffer v1Str, v2Str;
	size_t len;

	if (!jis_string(v1))
		throw "invalid value";
	if (!jis_string(v2))
		throw "invalid value2";

	v1Str = jstring_get_fast(v1);
	v2Str = jstring_get_fast(v2);
	len = v1Str.m_len < v2Str.m_len ? v1Str.m_len : v2Str.m_len;
	int comparison = memcmp(v1Str.m_str, v2Str.m_str, len);
	if (comparison == 0)
		return v1Str.m_len < v2Str.m_len;
	return comparison < 0;
}

namespace JSXMLConverter {

Namespace nsURI = { "http://palm.com/pbnjson", "jsxml" };
Namespace nsObj = { "http://palm.com/pbnjson/object", "obj" };
Namespace nsArr = { "http://palm.com/pbnjson/array", "arr" };
Namespace nsStr = { "http://palm.com/pbnjson/string", "str" };
Namespace nsNum = { "http://palm.com/pbnjson/number", "num" };
Namespace nsBool = { "http://palm.com/pbnjson/boolean", "bool" };

static void serialize(QDomNode &out, jvalue_ref jref, const Namespace &ns = nsURI, const QXmlAttributes &additionalAttributes = QXmlAttributes());
static void serializeObject(QDomNode &out, jvalue_ref jobj, const Namespace &ns, const QXmlAttributes &additionalAttributes);
static void serializeArray(QDomNode &out, jvalue_ref jarr, const Namespace &ns, const QXmlAttributes &additionalAttributes);
static void serializeValue(QDomNode &out, jvalue_ref jval, const Namespace &ns, const QXmlAttributes &additionalAttributes);

static QDomElement& apply(QDomElement& element, const QXmlAttributes &attrs)
{
	QDomDocument parent = element.ownerDocument();
	for (int i = 0; i < attrs.length(); i++) {
		QDomAttr attr = parent.createAttributeNS(attrs.uri(i), attrs.qName(i));
		attr.setValue(attrs.value(i));
		element.setAttributeNodeNS(attr);
	}
	return element;
}

static inline QString prefix(QString local, const Namespace &ns)
{
	return ns.prefix + ":" + local;
}

static __attribute__((unused)) QDomElement addElement(QDomNode &node, QString localName)
{
	Q_ASSERT(!node.isNull());
	QDomDocument parent = node.ownerDocument();
	QDomElement e = parent.createElement(localName);
	node.appendChild(e);
	return e;
}

static QDomElement addElement(QDomNode &node, QString localName, const Namespace &ns)
{
	Q_ASSERT(!node.isNull());
	QDomDocument parent = node.ownerDocument();
	QDomElement e = parent.createElement(prefix(localName, ns));
	node.appendChild(e);
	return e;
}

static inline QDomText addText(QDomElement &element, QString text)
{
	return element.ownerDocument().createTextNode(text);
}

static QDomCDATASection addCDATA(QDomElement& e, const raw_buffer &buf)
{
	QDomDocument parent = e.ownerDocument();
	QByteArray asData = QByteArray::fromRawData(buf.m_str, buf.m_len);
	return e.appendChild(parent.createCDATASection(asData)).toCDATASection();
}

static QDomText addText(QDomElement& e, const raw_buffer &buf)
{
	QDomDocument parent = e.ownerDocument();
	QByteArray asData = QByteArray::fromRawData(buf.m_str, buf.m_len);
	return e.appendChild(parent.createTextNode(asData)).toText();
}

template <class K, class V>
static inline void setAttribute(QDomElement &e, const Namespace &ns, const K &localName, const V &val)
{
	//e.setAttribute(prefix(localName, ns), val);
	QDomDocument parent = e.ownerDocument();
	QDomAttr attr = parent.createAttributeNS(ns.uri, prefix(localName, ns));
	attr.setValue(val);
	e.setAttributeNodeNS(attr);
}

template <class K>
static inline void setAttribute(QDomElement &e, const Namespace &ns, const K &localName, long long val)
{
	//e.setAttribute(prefix(localName, ns), val);
	QDomDocument parent = e.ownerDocument();
	QDomAttr attr = parent.createAttributeNS(ns.uri, prefix(localName, ns));
	attr.setValue(QString::number(val));
	e.setAttributeNodeNS(attr);
}


template <class K>
static inline void setAttribute(QDomElement &e, const Namespace &ns, const K &localName, int val)
{
	setAttribute(e, ns, localName, (long long)val);
}

static inline QXmlAttributes& addAttribute(QXmlAttributes& attrs, const Namespace &ns, QString localname, QString value)
{
	attrs.append(prefix(localname, ns), ns.uri, localname, value);
	return attrs;
}

static void serialize(QDomNode &out, jvalue_ref jref, const Namespace &ns, const QXmlAttributes &additionalAttributes)
{
	Q_ASSERT(!out.isNull());
	if (jis_object(jref))
		serializeObject(out, jref, ns, additionalAttributes);
	else if (jis_array(jref))
		serializeArray(out, jref, ns, additionalAttributes);
	else
		serializeValue(out, jref, ns, additionalAttributes);
}

static void serializeObject(QDomNode &out, jvalue_ref jobj, const Namespace &ns, const QXmlAttributes &additionalAttributes)
{
	// sort by key
	typedef QMap<JValueWrapper, jvalue_ref> JMap;
	JMap sorted;

	for (jobject_iter i = jobj_iter_init(jobj); jobj_iter_is_valid(i); i = jobj_iter_next(i)) {
		jobject_key_value keyval;
		if (!jobj_iter_deref(i, &keyval))
			throw "invalid iterator";
		sorted.insert(keyval.key, keyval.value);
	}

	Q_ASSERT(!out.isNull());
	QDomDocument parent = out.ownerDocument();
	QDomElement objElement = addElement(out, "object", ns);
	setAttribute(apply(objElement, additionalAttributes), nsObj, "length", (int)sorted.size());

	for (JMap::const_iterator i = sorted.constBegin(); i != sorted.constEnd(); i++) {
		QXmlAttributes valueAttributes;
		if (!jis_string(i.key()))
			throw "Invalid key";
		raw_buffer key = jstring_get_fast(i.key());

		if (strncmp(key.m_str, "test1", key.m_len) == 0)
			assert(i == sorted.constBegin());
		if (strncmp(key.m_str, "test2", key.m_len) == 0)
			assert(i != sorted.constBegin()); 
		addAttribute(valueAttributes, nsObj, "key", QByteArray::fromRawData(key.m_str, key.m_len));

		serialize(objElement, i.value(), nsObj, valueAttributes);
	}
}

static void serializeArray(QDomNode &out, jvalue_ref jarr, const Namespace &ns, const QXmlAttributes &additionalAttributes)
{
	QDomElement arrElement = addElement(out, "array", ns);
	setAttribute(apply(arrElement, additionalAttributes), nsArr, "length", (int64_t)jarray_size(jarr));

	for (ssize_t i = 0; i < jarray_size(jarr); i++) {
		QXmlAttributes valueAttributes;
		addAttribute(valueAttributes, nsArr, "index", QString::number(i));
		serialize(arrElement, jarray_get(jarr, i),  nsArr, valueAttributes);
	}
}

static void serializeValue(QDomNode &out, jvalue_ref jval, const Namespace &ns, const QXmlAttributes &additionalAttributes)
{
	QDomElement valueElement;

	if (jis_null(jval)) {
		valueElement = addElement(out, "null", ns);
	} else if (jis_string(jval)) {
		raw_buffer str = jstring_get_fast(jval);

		valueElement = addElement(out, "string", ns);
		setAttribute(valueElement, nsStr, "encoding", "unknown");
		setAttribute(valueElement, nsStr, "length", (int)str.m_len);
		addCDATA(valueElement, str);
	} else if (jis_number(jval)) {
		raw_buffer rawStr;
		int64_t integer;
		double floating;

		jnumber_get_raw(jval, &rawStr);
		valueElement = addElement(out, "number", ns);

		if (CONV_OK == jnumber_get_i64(jval, &integer)) {
			setAttribute(valueElement, nsNum, "format", "integer");
			addText(valueElement, QString::number(integer));
		} else if (CONV_OK == jnumber_get_f64(jval, &floating)) {
			setAttribute(valueElement, nsNum, "format", "floating");
			addText(valueElement, QString::number(floating));
		} else if (rawStr.m_str != NULL) {
			setAttribute(valueElement, nsNum, "format", "string");
			setAttribute(valueElement, nsNum, "length", (int)rawStr.m_len);
			addText(valueElement, rawStr);
		} else {
			throw "No storage type without some kind of error - huh?";
		}
	} else {
		bool truth;
		if (CONV_OK != jboolean_get(jval, &truth))
			throw "Unrecognized json value - not a null, string, number, or boolean";
		valueElement = addElement(out, "boolean", ns);
		addText(valueElement, (truth ? "true" : "false"));
	}
	out.appendChild(apply(valueElement, additionalAttributes));
}

QString xmlPreamble(QStringList preamble, QList<Namespace> lns)
{
	static QString xmlns("xmlns:");
	for (int i = 0; i < lns.size(); i++) {
		QStringList xmlnURI = QStringList() << xmlns << lns.at(i).prefix << "='" << lns.at(i).uri << "'";
		preamble << xmlnURI.join("");
	}
	return preamble.join(",");
}

QDomDocument convert(jvalue_ref json)
{
	QDomDocument out("xml");

	QDomNode processor = out.createProcessingInstruction("xml", "version='1.0' encoding='utf-8'");
	QDomElement root = out.createElement("jsxml");

	out.appendChild(root);
	out.insertBefore(processor, out.firstChild());

	setAttribute(root, nsURI, "library", "pbnjson");
	setAttribute(root, nsURI, "libVersion", "0.2");
	setAttribute(root, nsURI, "jsonEngine", "yajl");
	setAttribute(root, nsURI, "engineVersion", "");
	//defineNamespaces(out, root);

	Q_ASSERT(!root.isNull());
	Q_ASSERT(!out.isNull());
	serialize(root, json);

#if DEBUGGING_CONVERSION
	// for debugging - valgrind complains because technically this is not allowed
	// and too lazy to properly get the QByteArray utf8
	QString asQStr = out.toString();
	const char *asStr = qPrintable(asQStr);
#endif

	return out;

}

QDomDocument convert(QByteArray json)
{
	jvalue_ref parsed;

	JSchemaInfo info;
	jschema_info_init(&info, jschema_all(),
			NULL /* not a good idea - but we're using the empty schema anyways */,
			NULL /* use the default error handler - no recovery possible */);

	parsed = jdom_parse(j_str_to_buffer(json.constData(), json.size()), DOMOPT_INPUT_OUTLIVES_WITH_NOCHANGE, &info);
	if (jis_null(parsed)) {
		throw "invalid data";
	}
	QDomDocument converted = convert(parsed);
	j_release(&parsed);
	return converted;
}

static inline QString localName(const QDomElement &e)
{
	QStringList split = e.tagName().split(":");
	switch (split.size()) {
	case 1:
		qWarning() << "No namesapce for element" << e.tagName();
		return split[0];
	case 2:
	{
#if DEBUGGING_CONVERSION
	// for debugging - valgrind complains because technically this is not allowed
	// and too lazy to properly get the QByteArray utf8
		const char *ns = qPrintable(split[0]);
		const char *prop = qPrintable(split[1]);
#endif
		return split[1];
	}
	default:
		qWarning() << "Invalid split of element tag name" << e.tagName();
		return "";
	}
}

static inline QString prefix(const QDomElement &e)
{
	QStringList split = e.tagName().split(":");
	switch (split.size()) {
	case 1:
		qWarning() << "No namespace for element" << e.tagName();
		return "";
	case 2:
		return split[0];
	default:
		qWarning() << "Invalid split of element tag name" << e.tagName();
		return "";
	}
}

static __attribute__((unused)) void printAttributes(const char *name, QDomElement element)
{
	qDebug() << name << "element has following attributes:";
	QDomNamedNodeMap attributes = element.attributes();
	for (uint i = 0; i < attributes.length(); i++) {
		QDomNode attr = attributes.item(i);
		qDebug() << "with namespace" << attr.namespaceURI() << "and prefix" << attr.prefix() << " " << attr.nodeName() << "=" << attr.nodeValue();
	}
}

#define STRINGIFY(macro) #macro
static bool jdomEquivalent(const QDomElement &actual, const QDomElement &expected, int level = 0)
{
#define NECESSARY7(condition, msg, msg2, msg3, msg4, msg5, msg6, msg7) 									\
	do {																\
		if (!(condition)) { 													\
			qWarning() << STRINGIFY(condition) " failed at nesting" << level <<						\
				msg << msg2 << msg3 << msg4 << msg5 << msg6 << msg7							\
			;														\
			return false;													\
		}															\
	} while(0)

#define NECESSARY6(condition, msg, msg2, msg3, msg4, msg5, msg6) NECESSARY7(condition, msg, msg2, msg3, msg4, msg5, msg6, "")
#define NECESSARY5(condition, msg, msg2, msg3, msg4, msg5) NECESSARY6(condition, msg, msg2, msg3, msg4, msg5, "")
#define NECESSARY4(condition, msg, msg2, msg3, msg4) NECESSARY5(condition, msg, msg2, msg3, msg4, "")
#define NECESSARY3(condition, msg, msg2, msg3) NECESSARY4(condition, msg, msg2, msg3, "")
#define NECESSARY2(condition, msg, msg2) NECESSARY3(condition, msg, msg2, "")
#define NECESSARY1(condition, msg) NECESSARY2(condition, msg, "")
#define NECESSARY(condition) NECESSARY1(condition, "")
#define EQUIVALENT2(actual, expected) NECESSARY6(actual == expected, STRINGIFY(actual) " != " STRINGIFY(expected), "(", (actual), "vs", (expected), ")" )
#define EQUIVALENT(actual, expected, tagname) NECESSARY7(actual == expected, STRINGIFY(actual) " != " STRINGIFY(expected), "(", (actual), "vs", (expected), ") for tag", tagname )
#define EQ_ATTR(a, e, attr) EQUIVALENT(a.attr, e.attr, a.tagName())
#define EQ_ATTR2(attr) EQ_ATTR(actual, expected, attr)
#define EQ_ATTR3(ns, key) EQ_ATTR2(attributeNS(ns.uri, key))
#define EQ_ATTR_C2(attr) EQ_ATTR(aChild, eChild, attr)
#define EQ_ATTR_C3(ns, key) EQ_ATTR_C2(attributeNS(ns.uri, key))
#define LOCAL_NAME(e) (e.split(":")[1])
#define PREFIX(e) (e.split(":")[0])

	EQUIVALENT2(actual.tagName(), expected.tagName());

	QString actualPrefix = prefix(actual);
	QString expectedPrefix = prefix(expected);
	QString actualLocalName = localName(actual);
	QString expectedLocalName = localName(expected);

	if (level != 0) {
		NECESSARY5(actualPrefix == nsObj.prefix || actualPrefix == nsArr.prefix, actual.tagName(), ": prefix", actualPrefix, ", local", actualLocalName);
		NECESSARY5(expectedPrefix == nsObj.prefix || expectedPrefix == nsArr.prefix, expected.tagName(), ": prefix", expectedPrefix, ", local", expectedLocalName);
	} else {
		NECESSARY5(actualPrefix == nsURI.prefix, actual.tagName(), ": prefix", actualPrefix, ", local", actualLocalName);
		NECESSARY5(expectedPrefix == nsURI.prefix, expected.tagName(), ": prefix", expectedPrefix, ", local", expectedLocalName);
	}

	if (actualLocalName == "object") {
		NECESSARY(expected.hasAttributeNS(nsObj.uri, "length"));
		NECESSARY(actual.hasAttributeNS(nsObj.uri, "length"));
		int numChildren = actual.attributeNS(nsObj.uri, "length", "0").toInt();
		int actualChildren = 0;
		EQ_ATTR3(nsObj, "length");
		QDomElement aChild = actual.firstChildElement();
		QDomElement eChild = expected.firstChildElement();
		while (!aChild.isNull()) {
			NECESSARY(!eChild.isNull());

			actualChildren++;
			NECESSARY(aChild.attributeNS(nsObj.uri, "key", "") != "");
			EQ_ATTR_C3(nsObj, "key");
			if (!jdomEquivalent(aChild, eChild, level + 1))
				return false;
			aChild = aChild.nextSiblingElement();
			eChild = eChild.nextSiblingElement();
		}
		EQUIVALENT(actualChildren, numChildren, actual.tagName());
	} else if (actualLocalName == "array") {
		int numChildren = actual.attributeNS(nsArr.uri, "length", "0").toInt();
		NECESSARY(expected.hasAttributeNS(nsArr.uri, "length"));
		NECESSARY(actual.hasAttributeNS(nsArr.uri, "length"));
		int actualChildren = 0;
		EQ_ATTR3(nsArr, "length");
		QDomElement aChild = actual.firstChildElement();
		QDomElement eChild = actual.firstChildElement();
		while (!aChild.isNull()) {
			NECESSARY(!eChild.isNull());

			actualChildren++;
			NECESSARY(aChild.attributeNS(nsArr.uri, "index", "") != "");
			EQ_ATTR_C3(nsArr, "index");
			if (!jdomEquivalent(aChild, eChild, level + 1))
				return false;
			aChild = aChild.nextSiblingElement();
			eChild = eChild.nextSiblingElement();
		}
		EQUIVALENT(actualChildren, numChildren, actual.tagName());
	} else {
		if (actualLocalName == "string") {
			NECESSARY(actual.attributeNS(nsStr.uri, "encoding", "unknown") == expected.attributeNS(nsStr.uri, "encoding", "unknown"));
			EQ_ATTR3(nsStr, "length");
		} else if (actualLocalName == "number") {
			EQ_ATTR3(nsNum, "format");
		} else if (actualLocalName == "boolean") {
		} else if (actualLocalName == "null") {
		} else {
			NECESSARY2(false, "Unrecognized name", actualLocalName);
		}
		EQUIVALENT(actual.text(), expected.text(), actual.tagName());
	}

#undef NECESSARY6
#undef NECESSARY
#undef EQUIVALENT
#undef EQ_ATTR
#undef EQ_ATTR2
#undef EQ_ATTR3

	return true;
}

bool domEquivalent(const QDomDocument &actual, const QDomDocument &expected)
{
#define NECESSARY6(condition, msg, msg2, msg3, msg4, msg5, msg6) do { if (!(condition)) { qWarning() << STRINGIFY(condition) " failed" << msg << msg2 << msg3 << msg4 << msg5 << msg6; return false; } } while(0)
#define NECESSARY5(condition, msg, msg2, msg3, msg4, msg5) NECESSARY6(condition, msg, msg2, msg3, msg4, msg5, "")
#define NECESSARY4(condition, msg, msg2, msg3, msg4) NECESSARY5(condition, msg, msg2, msg3, msg4, "")
#define NECESSARY3(condition, msg, msg2, msg3) NECESSARY4(condition, msg, msg2, msg3, "")
#define NECESSARY2(condition, msg, msg2) NECESSARY3(condition, msg, msg2, "")
#define NECESSARY1(condition, msg) NECESSARY2(condition, msg, "")
#define NECESSARY(condition) NECESSARY1(condition, "")
#define EQUIVALENT(actual, expected) NECESSARY6(actual == expected, STRINGIFY(actual) " != " STRINGIFY(expected), "(", (actual), "vs", (expected), ")" )
#define EQ_ATTR(a, e, attr) EQUIVALENT(a.attr, e.attr)
#define EQ_ATTR2(attr) EQ_ATTR(aRoot, eRoot, attr)
#define EQ_ATTR3(ns, key) EQ_ATTR2(attributeNS(ns.uri, key))
#define EQ_ATTR4(key) EQ_ATTR3(nsURI, key)

	QDomElement aRoot, eRoot;
	aRoot = actual.documentElement(); eRoot = actual.documentElement();

	EQUIVALENT(aRoot.tagName(), eRoot.tagName());
	EQ_ATTR4("library");
	EQ_ATTR4("libVersion");
	EQ_ATTR4("obj");
	EQ_ATTR4("arr");
	EQ_ATTR4("str");
	EQ_ATTR4("num");
	EQ_ATTR4("bool");

#if DEBUGGING_CONVERSION
	// for debugging - valgrind complains because technically this is not allowed
	// and too lazy to properly get the QByteArray utf8
	const char *actualStr, *expectedStr;
	QString actualQStr = actual.toString();
	QString expectedQStr = expected.toString();
	actualStr = qPrintable(actualQStr);
	expectedStr = qPrintable(expectedQStr);
#endif

	QDomElement aJRoot, eJRoot;
	aJRoot = aRoot.firstChildElement();
	eJRoot = eRoot.firstChildElement();

	NECESSARY(!aJRoot.isNull());
	Q_ASSERT(!eJRoot.isNull());

	EQUIVALENT(aJRoot.tagName(), eJRoot.tagName());
	Q_ASSERT(eJRoot.tagName() == "jsxml:object" || eJRoot.tagName() == "jsxml:array");
	Q_ASSERT(aJRoot.tagName() == "jsxml:object" || aJRoot.tagName() == "jsxml:array");

#undef NECESSARY6
#undef NECESSARY
#undef EQUIVALENT
#undef EQ_ATTR
#undef EQ_ATTR2
#undef EQ_ATTR3
#undef EQ_ATTR4

	return jdomEquivalent(aJRoot, eJRoot);
}

}
