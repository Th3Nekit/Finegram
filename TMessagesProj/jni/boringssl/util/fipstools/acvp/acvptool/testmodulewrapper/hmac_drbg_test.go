// Copyright 2021 The BoringSSL Authors
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

package main

import (
	"encoding/hex"
	"testing"
)

func TestHMACDRBG(t *testing.T) {
	drbg := NewHMACDRBG(fromHex("06032cd5eed33f39265f49ecb142c511da9aff2af71203bffaf34a9ca5bd9c0d"),
		fromHex("0e66f71edc43e42a45ad3c6fc6cdc4df"),
		nil)

	drbg.Reseed(fromHex("01920a4e669ed3a85ae8a33b35a74ad7fb2a6bb4cf395ce00334a9c9a5a5d552"), nil)

	var out [1024 / 8]byte
	drbg.Generate(out[:], nil)
	drbg.Generate(out[:], nil)

	if hex.EncodeToString(out[:]) != "76fc79fe9b50beccc991a11b5635783a83536add03c157fb30645e611c2898bb2b1bc215000209208cd506cb28da2a51bdb03826aaf2bd2335d576d519160842e7158ad0949d1a9ec3e66ea1b1a064b005de914eac2e9d4f2d72a8616a80225422918250ff66a41bd2f864a6a38cc5b6499dc43f7f2bd09e1e0f8f5885935124" {
		t.Errorf("Incorrect result: %x", out)
	}
}

func TestHMACDRBGPredictionResistance(t *testing.T) {
	drbg := NewHMACDRBG(fromHex("6c1f4bffc476e488fb57eb80dc106cf2b417bad22b196baa6346958256db490f"),
		fromHex("5f1b92223e3909e43677da2f588a6d19"),
		nil)

	var out [32]byte
	drbg.Reseed(fromHex("abaca65695bd5d289880453850fc8289b76f78b43f970ed32f4125a941165515"), fromHex("e6cd940610375e504fa80406120b34d498b022393436e910c0ba2560603fd066"))
	drbg.Generate(out[:], nil)
	drbg.Reseed(fromHex("4a39b666cf861816d7d82ef6e23f70f149d74d9bd499eea19b622e751c43d839"), fromHex("d20082c5bdf6f6711af391e7d01046b9d3610827de63aa2671a5f5ad07b90841"))
	drbg.Generate(out[:], nil)

	if hex.EncodeToString(out[:]) != "d3c36e4ae25ff21a95a157a89f13eb976362a695ea755f0465ed4a7bb20c5cb3" {
		t.Errorf("Incorrect result: %x", out)
	}
}

func fromHex(h string) []byte {
	ret, err := hex.DecodeString(h)
	if err != nil {
		panic(err)
	}
	return ret
}
