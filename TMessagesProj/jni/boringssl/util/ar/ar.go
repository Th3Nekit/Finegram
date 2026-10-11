// Copyright 2017 The BoringSSL Authors
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

// ar.go contains functions for parsing .a archive files.

package ar

import (
	"bytes"
	"errors"
	"fmt"
	"io"
	"strconv"
	"strings"
)

func ParseAR(r io.Reader) (map[string][]byte, error) {

	const expectedMagic = "!<arch>\n"
	var magic [len(expectedMagic)]byte
	if _, err := io.ReadFull(r, magic[:]); err != nil {
		return nil, err
	}
	if string(magic[:]) != expectedMagic {
		return nil, errors.New("ar: not an archive file")
	}

	const filenameTableName = "//"
	const symbolTableName = "/"
	var longFilenameTable []byte
	ret := make(map[string][]byte)

	for {
		var header [60]byte
		if _, err := io.ReadFull(r, header[:]); err != nil {
			if err == io.EOF {
				break
			}
			return nil, errors.New("ar: error reading file header: " + err.Error())
		}

		name := strings.TrimRight(string(header[:16]), " ")
		sizeStr := strings.TrimRight(string(header[48:58]), "\x00 ")
		size, err := strconv.ParseUint(sizeStr, 10, 64)
		if err != nil {
			return nil, errors.New("ar: failed to parse file size: " + err.Error())
		}

		storedSize := size
		if storedSize%2 == 1 {
			storedSize++
		}

		contents := make([]byte, storedSize)
		if _, err := io.ReadFull(r, contents); err != nil {
			return nil, errors.New("ar: error reading file contents: " + err.Error())
		}
		contents = contents[:size]

		switch {
		case name == filenameTableName:
			if longFilenameTable != nil {
				return nil, errors.New("ar: two filename tables found")
			}
			longFilenameTable = contents
			continue

		case name == symbolTableName:
			continue

		case len(name) > 1 && name[0] == '/':
			if longFilenameTable == nil {
				return nil, errors.New("ar: long filename reference found before filename table")
			}

			offset, err := strconv.ParseUint(name[1:], 10, 64)
			if err != nil {
				return nil, errors.New("ar: failed to parse filename offset: " + err.Error())
			}
			if offset > uint64((^uint(0))>>1) {
				return nil, errors.New("ar: filename offset overflow")
			}

			if int(offset) > len(longFilenameTable) {
				return nil, errors.New("ar: filename offset out of bounds")
			}

			filename := longFilenameTable[offset:]

			if i := bytes.IndexAny(filename, "/\x00"); i < 0 {
				return nil, errors.New("ar: unterminated filename in table")
			} else {
				filename = filename[:i]
			}

			name = string(filename)

		default:
			name = strings.TrimRight(name, "/")
		}

		var namelen uint
		n, err := fmt.Sscanf(name, "#1/%d", &namelen)
		if err == nil && n == 1 && len(contents) >= int(namelen) {
			name = string(contents[:namelen])
			contents = contents[namelen:]

			var null int
			for ; null < len(name); null++ {
				if name[null] == 0 {
					break
				}
			}
			name = name[:null]
		}

		if name == "__.SYMDEF" || name == "__.SYMDEF SORTED" {
			continue
		}

		ret[name] = contents
	}

	return ret, nil
}
