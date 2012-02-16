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

#include <pbnjson.hpp>

#define STOP_LOOP break

using namespace std;
using namespace pbnjson;

static bool simpleJsonTest()
{
	string jsonRaw = "{\"args\":[17]}";
	JDomParser parser(NULL);
	if (! parser.parse(jsonRaw, JSchemaFragment("{}"))) {
		return false;
	}
	JValue json = parser.getDom();
	JValue args = json["args"];
	if (args.isNull() || (! args.isArray())) {
		return false;
	}
	pbnjson::JValue param0 = args[0];
	double time;
	if (CONV_OK != param0.asNumber(time)) {
	    return false;
	}
	return true;
}

int main(int argc, char** argv)
{
	bool ok = true;
#ifdef LOOP_FOREVER
	while(1)
#elif defined(LOOP_MULTIPLE)
	const int num_loops = 5000;
	int j;
	for (j = 0; j < num_loops; ++j)
#else
#undef STOP_LOOP
#define STOP_LOOP do {} while (0)
#endif
	{
		if (!simpleJsonTest()) {
			cerr << "Failed to decode json" << endl;
			ok = false;
			STOP_LOOP;
		}
	}
	return ok != true;
}

