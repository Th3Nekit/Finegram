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

func addPeekTests() {

	testCases = append(testCases, testCase{
		name:             "Peek-Basic",
		sendEmptyRecords: 1,
		flags:            []string{"-peek-then-read"},
	})

	testCases = append(testCases, testCase{
		name: "Peek-ImplicitHandshake",
		flags: []string{
			"-peek-then-read",
			"-implicit-handshake",
		},
	})

	testCases = append(testCases, testCase{
		name: "Peek-Renegotiate",
		config: Config{
			MaxVersion: VersionTLS12,
		},
		renegotiate: 1,
		flags: []string{
			"-peek-then-read",
			"-renegotiate-freely",
			"-expect-total-renegotiations", "1",
		},
	})

	testCases = append(testCases, testCase{
		name: "Peek-Shutdown",
		config: Config{
			Bugs: ProtocolBugs{
				ExpectCloseNotify: true,
			},
		},
		flags: []string{
			"-peek-then-read",
			"-check-close-notify",
		},
	})

	testCases = append(testCases, testCase{
		name: "Peek-Alert",
		config: Config{
			Bugs: ProtocolBugs{
				SendSpuriousAlert: alertRecordOverflow,
			},
		},
		flags:         []string{"-peek-then-read"},
		shouldFail:    true,
		expectedError: ":TLSV1_ALERT_RECORD_OVERFLOW:",
	})

	testCases = append(testCases, testCase{
		name: "Peek-KeyUpdate",
		config: Config{
			MaxVersion: VersionTLS13,
		},
		sendKeyUpdates:   1,
		keyUpdateRequest: keyUpdateNotRequested,
		flags:            []string{"-peek-then-read"},
	})
}
