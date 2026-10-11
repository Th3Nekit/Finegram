// Copyright 2019 The BoringSSL Authors
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

package subprocess

import (
	"encoding/hex"
	"encoding/json"
	"fmt"
)

type hashTestVectorSet struct {
	Groups []hashTestGroup `json:"testGroups"`
}

type hashTestGroup struct {
	ID    uint64 `json:"tgId"`
	Type  string `json:"testType"`
	Tests []struct {
		ID        uint64 `json:"tcId"`
		BitLength uint64 `json:"len"`
		MsgHex    string `json:"msg"`
	} `json:"tests"`
}

type hashTestGroupResponse struct {
	ID    uint64             `json:"tgId"`
	Tests []hashTestResponse `json:"tests"`
}

type hashTestResponse struct {
	ID         uint64          `json:"tcId"`
	DigestHex  string          `json:"md,omitempty"`
	MCTResults []hashMCTResult `json:"resultsArray,omitempty"`
}

type hashMCTResult struct {
	DigestHex string `json:"md"`
}

type hashPrimitive struct {

	algo string

	size int
}

func (h *hashPrimitive) Process(vectorSet []byte, m Transactable) (any, error) {
	var parsed hashTestVectorSet
	if err := json.Unmarshal(vectorSet, &parsed); err != nil {
		return nil, err
	}

	var ret []hashTestGroupResponse

	for _, group := range parsed.Groups {
		group := group
		response := hashTestGroupResponse{
			ID: group.ID,
		}

		for _, test := range group.Tests {
			test := test

			if uint64(len(test.MsgHex))*4 != test.BitLength {
				return nil, fmt.Errorf("test case %d/%d contains hex message of length %d but specifies a bit length of %d", group.ID, test.ID, len(test.MsgHex), test.BitLength)
			}
			msg, err := hex.DecodeString(test.MsgHex)
			if err != nil {
				return nil, fmt.Errorf("failed to decode hex in test case %d/%d: %s", group.ID, test.ID, err)
			}

			switch group.Type {
			case "AFT":
				m.TransactAsync(h.algo, 1, [][]byte{msg}, func(result [][]byte) error {
					response.Tests = append(response.Tests, hashTestResponse{
						ID:        test.ID,
						DigestHex: hex.EncodeToString(result[0]),
					})
					return nil
				})

			case "MCT":
				if len(msg) != h.size {
					return nil, fmt.Errorf("MCT test case %d/%d contains message of length %d but the digest length is %d", group.ID, test.ID, len(msg), h.size)
				}

				testResponse := hashTestResponse{ID: test.ID}

				digest := msg
				for i := 0; i < 100; i++ {
					result, err := m.Transact(h.algo+"/MCT", 1, digest)
					if err != nil {
						panic(h.algo + " hash operation failed: " + err.Error())
					}

					digest = result[0]
					testResponse.MCTResults = append(testResponse.MCTResults, hashMCTResult{hex.EncodeToString(digest)})
				}

				response.Tests = append(response.Tests, testResponse)

			default:
				return nil, fmt.Errorf("test group %d has unknown type %q", group.ID, group.Type)
			}
		}

		m.Barrier(func() {
			ret = append(ret, response)
		})
	}

	if err := m.Flush(); err != nil {
		return nil, err
	}

	return ret, nil
}
