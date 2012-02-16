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

/*
 * TestYAJL.h
 *
 *  Created on: Sep 22, 2009
 */

#ifndef TESTYAJL_H_
#define TESTYAJL_H_

#include <QObject>

namespace pjson {

namespace test {

class TestYAJL : public QObject
{
	Q_OBJECT

public:
	TestYAJL();
	virtual ~TestYAJL();

private slots:
	void testGenerator();
	void testParser_data();
	void testParser();
};

}

}

#endif /* TESTYAJL_H_ */
