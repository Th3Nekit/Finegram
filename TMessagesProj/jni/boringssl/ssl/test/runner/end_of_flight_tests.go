// Copyright 2025 The BoringSSL Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package runner

func addEndOfFlightTests() {

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "PartialSecondClientHelloAfterFirst",
		config: Config{
			MaxVersion: VersionTLS13,

			DefaultCurves: []CurveID{},
			Bugs: ProtocolBugs{
				PartialSecondClientHelloAfterFirst: true,
			},
		},
		shouldFail:         true,
		expectedError:      ":EXCESS_HANDSHAKE_DATA:",
		expectedLocalError: "remote error: unexpected message",
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "PartialServerHelloWithHelloRetryRequest",
		config: Config{
			MaxVersion: VersionTLS13,

			CurvePreferences: []CurveID{CurveP384},
			Bugs: ProtocolBugs{
				PartialServerHelloWithHelloRetryRequest: true,
			},
		},
		shouldFail:         true,
		expectedError:      ":EXCESS_HANDSHAKE_DATA:",
		expectedLocalError: "remote error: unexpected message",
	})

	testCases = append(testCases, testCase{
		testType: serverTest,
		name:     "PartialClientKeyExchangeWithClientHello",
		config: Config{
			MaxVersion: VersionTLS12,
			Bugs: ProtocolBugs{
				PartialClientKeyExchangeWithClientHello: true,
			},
		},
		shouldFail:         true,
		expectedError:      ":EXCESS_HANDSHAKE_DATA:",
		expectedLocalError: "remote error: unexpected message",
	})

	testCases = append(testCases, testCase{
		testType: clientTest,
		name:     "PartialNewSessionTicketWithServerHelloDone",
		config: Config{
			MaxVersion: VersionTLS12,
			Bugs: ProtocolBugs{
				PartialNewSessionTicketWithServerHelloDone: true,
			},
		},
		shouldFail:         true,
		expectedError:      ":EXCESS_HANDSHAKE_DATA:",
		expectedLocalError: "remote error: unexpected message",
	})

	for _, vers := range tlsVersions {
		for _, testType := range []testType{clientTest, serverTest} {
			suffix := "-Client"
			if testType == serverTest {
				suffix = "-Server"
			}
			suffix += "-" + vers.name

			testCases = append(testCases, testCase{
				testType: testType,
				name:     "TrailingDataWithFinished" + suffix,
				config: Config{
					MaxVersion: vers.version,
					Bugs: ProtocolBugs{
						TrailingDataWithFinished: true,
					},
				},
				shouldFail:         true,
				expectedError:      ":EXCESS_HANDSHAKE_DATA:",
				expectedLocalError: "remote error: unexpected message",
			})
			testCases = append(testCases, testCase{
				testType: testType,
				name:     "TrailingDataWithFinished-Resume" + suffix,
				config: Config{
					MaxVersion: vers.version,
				},
				resumeConfig: &Config{
					MaxVersion: vers.version,
					Bugs: ProtocolBugs{
						TrailingDataWithFinished: true,
					},
				},
				resumeSession:      true,
				shouldFail:         true,
				expectedError:      ":EXCESS_HANDSHAKE_DATA:",
				expectedLocalError: "remote error: unexpected message",
			})
		}
	}
}
