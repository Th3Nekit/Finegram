/*
 * Copyright 2013-2014 Odysseus Software GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package org.telegram.messenger.audioinfo.mp3;

import org.telegram.messenger.audioinfo.util.PositionInputStream;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;

public class ID3v2TagHeader {
	private int version = 0;
	private int revision = 0;
	private int headerSize = 0;
	private int totalTagSize = 0;
	private int paddingSize = 0;
	private int footerSize = 0;
	private boolean unsynchronization;
	private boolean compression;

	public ID3v2TagHeader(InputStream input) throws IOException, ID3v2Exception {
		this(new PositionInputStream(input));
	}

	ID3v2TagHeader(PositionInputStream input) throws IOException, ID3v2Exception {
		long startPosition = input.getPosition();

		ID3v2DataInput data = new ID3v2DataInput(input);

		String id = new String(data.readFully(3), "ISO-8859-1");
		if (!"ID3".equals(id)) {
			throw new ID3v2Exception("Invalid ID3 identifier: " + id);
		}

		version = data.readByte();
		if (version != 2 && version != 3 && version != 4) {
			throw new ID3v2Exception("Unsupported ID3v2 version: " + version);
		}

		revision = data.readByte();

		byte flags = data.readByte();

		totalTagSize = 10 + data.readSyncsafeInt();

		if (version == 2) {
			unsynchronization = (flags & 0x80) != 0;
			compression = (flags & 0x40) != 0;
		} else {
			unsynchronization = (flags & 0x80) != 0;

			if ((flags & 0x40) != 0) {
				if (version == 3) {

					int extendedHeaderSize = data.readInt();

					data.readByte();
					data.readByte();

					paddingSize = data.readInt();

					data.skipFully(extendedHeaderSize - 6);
				} else {

					int extendedHeaderSize = data.readSyncsafeInt();

					data.skipFully(extendedHeaderSize - 4);
				}
			}

			if (version >= 4 && (flags & 0x10) != 0) {
				footerSize = 10;
				totalTagSize += 10;
			}
		}

		headerSize = (int) (input.getPosition() - startPosition);
	}

	public ID3v2TagBody tagBody(InputStream input) throws IOException, ID3v2Exception {
		if (compression) {
			throw new ID3v2Exception("Tag compression is not supported");
		}
		if (version < 4 && unsynchronization) {
			byte[] bytes = new ID3v2DataInput(input).readFully(totalTagSize - headerSize);
			boolean ff = false;
			byte ffByte = (byte) 0xFF;
			int len = 0;
			for (byte b : bytes) {
				if (!ff || b != 0) {
					bytes[len++] = b;
				}
				ff = (b == ffByte);
			}
			return new ID3v2TagBody(new ByteArrayInputStream(bytes, 0, len), headerSize, len, this);
		} else {
			return new ID3v2TagBody(input, headerSize, totalTagSize - headerSize - footerSize, this);
		}
	}

	public int getVersion() {
		return version;
	}

	public int getRevision() {
		return revision;
	}

	public int getTotalTagSize() {
		return totalTagSize;
	}

	public boolean isUnsynchronization() {
		return unsynchronization;
	}

	public boolean isCompression() {
		return compression;
	}

	public int getHeaderSize() {
		return headerSize;
	}

	public int getFooterSize() {
		return footerSize;
	}

	public int getPaddingSize() {
		return paddingSize;
	}

	@Override
	public String toString() {
		return String.format("%s[version=%s, totalTagSize=%d]", getClass().getSimpleName(), version, totalTagSize);
	}
}
