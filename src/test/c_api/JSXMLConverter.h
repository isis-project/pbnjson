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

#ifndef JSXML_CONVERTER_H_
#define JSXML_CONVERTER_H_

#include <QByteArray>
#include <QDomDocument>
#include <pbnjson.h>

namespace JSXMLConverter {

struct Namespace {
	QString uri;
	QString prefix;
};

extern Namespace nsURI;
extern Namespace nsObj;
extern Namespace nsArr;
extern Namespace nsStr;
extern Namespace nsNum;
extern Namespace nsBool;

QDomDocument convert(QByteArray json);
QDomDocument convert(jvalue_ref json);

bool domEquivalent(const QDomDocument &actual, const QDomDocument &expected);

}

class JValueWrapper
{
public:
	JValueWrapper(jvalue_ref ref) : m_ref(ref) {}
	JValueWrapper() : m_ref(NULL) {}
	JValueWrapper(const JValueWrapper& c) : m_ref(c.m_ref) {}
	~JValueWrapper(){}

	JValueWrapper& operator=(const JValueWrapper& other){
		if (this != &other) m_ref = other.m_ref;
		return *this;
	}

	operator jvalue_ref() const {
		return m_ref;
	}

private:
	jvalue_ref m_ref;
};

bool operator<(const JValueWrapper &v1w, const JValueWrapper &v2w);

#endif /* JSXML_CONVERTER_H_ */
