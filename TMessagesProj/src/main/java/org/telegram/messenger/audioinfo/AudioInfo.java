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
package org.telegram.messenger.audioinfo;

import android.graphics.Bitmap;
import android.media.MediaMetadataRetriever;

import org.telegram.messenger.audioinfo.m4a.M4AInfo;
import org.telegram.messenger.audioinfo.mp3.MP3Info;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.io.RandomAccessFile;

public abstract class AudioInfo {
	protected String brand;			                                
	protected String version;		                                   

	protected long duration;		                                

	protected String title;			              
	protected String artist;		               
	protected String albumArtist;	               
	protected String album;			              
	protected short year;			          
	protected String genre;			             
	protected String comment;		             
	protected short track;			               
	protected short tracks;			                   
	protected short disc;			              
	protected short discs;			                  
	protected String copyright;		// copyright notice
	protected String composer;		                
	protected String grouping;		                 
	protected boolean compilation;	                   
	protected String lyrics;		              
	protected Bitmap cover;			                   
    protected Bitmap smallCover;	                   

	private File coverFile;

	public String getBrand() {
		return brand;
	}

	public String getVersion() {
		return version;
	}

	public long getDuration() {
		return duration;
	}

	public String getTitle() {
		return title;
	}

	public String getArtist() {
		return artist;
	}

	public String getAlbumArtist() {
		return albumArtist;
	}

	public String getAlbum() {
		return album;
	}

	public short getYear() {
		return year;
	}

	public String getGenre() {
		return genre;
	}

	public String getComment() {
		return comment;
	}

	public short getTrack() {
		return track;
	}

	public short getTracks() {
		return tracks;
	}

	public short getDisc() {
		return disc;
	}

	public short getDiscs() {
		return discs;
	}

	public String getCopyright() {
		return copyright;
	}

	public String getComposer() {
		return composer;
	}

	public String getGrouping() {
		return grouping;
	}

	public boolean isCompilation() {
		return compilation;
	}

	public String getLyrics() {
		return lyrics;
	}

	public Bitmap getCover() {
		return cover;
	}

	public File getCoverFile() {
		return coverFile;
	}

	public void setCoverFile(File file) {
		coverFile = file;
	}

    public Bitmap getSmallCover() {
        return smallCover;
    }

	public static AudioInfo getAudioInfo(File file) {
        try {
            byte header[] = new byte[12];
            RandomAccessFile randomAccessFile = new RandomAccessFile(file, "r");
            randomAccessFile.readFully(header, 0, 8);
            randomAccessFile.close();
            InputStream input = new BufferedInputStream(new FileInputStream(file));
            if (header[4] == 'f' && header[5] == 't' && header[6] == 'y' && header[7] == 'p') {
                return new M4AInfo(input);
            } else if (header[0] == 'f' && header[1] == 'L' && header[2] == 'a' && header[3] == 'c') {
				OtherAudioInfo info = new OtherAudioInfo(file);
				if (info.failed) return null;
				return info;
			} else if (file.getAbsolutePath().endsWith("mp3") || (
				(header[0] == 'I' && header[1] == 'D' && header[2] == '3') ||
				(header[0] == 'T' && header[1] == 'A' && header[2] == 'G')
			)) {
                return new MP3Info(input, file.length());
            } else {
				OtherAudioInfo info = new OtherAudioInfo(file);
				if (info.failed) return null;
				return info;
			}
        } catch (Exception e) {}
		return null;
    }
}
