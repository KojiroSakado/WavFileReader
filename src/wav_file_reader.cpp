#define _CRT_SECURE_NO_WARNINGS//without this, Visual Studio throws warnings for fopen function that suggests to use fopen_s instead
#include<cstdio>
#include<cstdlib>
#include<cstring>
#include"wav_file_reader.h"

namespace gold {


	WavFileReader::WavFileReader(const char* filename, unsigned int gpBufCnt) {
		this->numGpBuf = gpBufCnt;
		WavFileReaderPrivate(filename);
	}

	WavFileReader::WavFileReader(const char*filename) {
		numGpBuf = 44100;//default
		WavFileReaderPrivate(filename);
	}

	void WavFileReader::WavFileReaderPrivate(const char*filename) {

		char ch[5];
		unsigned int size;//chunk size

		fp = fopen(filename, "rb");
		if (fp == NULL) {
			throw WFRFileOpenException();
		}

		fread(ch, 1, 4, fp);
		ch[4] = '\0';
		if (strcmp(ch, "RIFF")) {
			fclose(fp);
			throw WFRFileValidityException();
		}

		fseek(fp, 4, SEEK_CUR);
		fread(ch, 1, 4, fp);
		ch[4] = '\0';
		if (strcmp(ch, "WAVE")) {
			fclose(fp);
			throw WFRFileValidityException();
		}

		fseek(fp, 4, SEEK_CUR);
		fread(&FmtSize, 4, 1, fp);
		fread(&FmtID, 2, 1, fp);
		fread(&NumChannels, 2, 1, fp);
		fread(&SampleRate, 4, 1, fp);
		fread(&BytesPerSec, 4, 1, fp);
		fread(&BlockAlign, 2, 1, fp);
		fread(&BitsPerSample, 2, 1, fp);
		fseek(fp, FmtSize - 16, SEEK_CUR);
		fread(ch, 1, 4, fp);
		while (strcmp(ch, "data")) {
			if (fread(&size, 4, 1, fp) != 1) {
				fclose(fp);
				throw WFRFileValidityException();
			}
			fseek(fp, size, SEEK_CUR);
			fread(ch, 1, 4, fp);
		}
		fread(&DataSize, 4, 1, fp);
		BytesPerSample = BitsPerSample / 8;
		NumData = DataSize / BlockAlign;

		if (BitsPerSample != 8 && BitsPerSample != 16) {
			fclose(fp);
			throw WFRSupportedFormatException();
		}

		if (NumChannels != 1 && NumChannels != 2) {
			fclose(fp);
			throw WFRFileValidityException();
		}

		dataCnt = 0;

		gpBuf = (unsigned char*)malloc(BytesPerSample * numGpBuf * NumChannels);
		ucharp = (unsigned char*)gpBuf;
		shortp = (signed short int*)gpBuf;

		return;
	}

	void WavFileReader::PrintHeader() {

		printf("format size= %d\nformat ID = %d\nchannels = %d\nsampling rate = %d\nbytes per sec = %d\nblock align = %d\nbits per sample = %d\ndata size = %d\n"
			, FmtSize, FmtID, NumChannels, SampleRate, BytesPerSec, BlockAlign, BitsPerSample, DataSize);

		return;
	}

	WavFileReader::~WavFileReader() {
		fclose(fp);
		free(gpBuf);
	}

	int WavFileReader::Read(unsigned char *buf, unsigned int leftToRead) {

		unsigned int readCnt, numSuccess, i = 0, pointer = 0;

		if (dataCnt + leftToRead > NumData)leftToRead = NumData - dataCnt;
		readCnt = numGpBuf * NumChannels;

		while (leftToRead > 0) {

			if (leftToRead < numGpBuf) {
				readCnt = leftToRead * NumChannels;
				leftToRead = 0;
			}
			else {
				leftToRead -= numGpBuf;
			}

			numSuccess = fread(gpBuf, BytesPerSample, readCnt, fp);
			if (numSuccess < readCnt) leftToRead = 0;

			if (NumChannels == 1 && BytesPerSample == 1) {
				memcpy(buf + pointer, ucharp, numSuccess);
				pointer += numSuccess;
			}
			else if (NumChannels == 1 && BytesPerSample == 2) {
				for (i = 0; i < numSuccess; pointer++, i++) {
					buf[pointer] = (unsigned int)((int)shortp[i] + 0x8000) >> 9;
				}
			}
			else if (NumChannels == 2 && BytesPerSample == 1) {
				for (i = 0; i < numSuccess; pointer++, i += 2) {
					buf[pointer] = ((unsigned int)ucharp[i] + (unsigned int)ucharp[i + 1]) >> 1;
				}
			}
			else if (NumChannels == 2 && BytesPerSample == 2) {
				for (i = 0; i < numSuccess; pointer++, i += 2) {
					buf[pointer] = (unsigned int)((int)shortp[i] + (int)shortp[i + 1] + 0x10000) >> 9;
				}
			}
		}

		dataCnt += pointer;
		return pointer;
	}

	int WavFileReader::Read(signed short *buf, unsigned int leftToRead) {

		unsigned int readCnt, numSuccess, i = 0, pointer = 0;

		if (dataCnt + leftToRead > NumData)leftToRead = NumData - dataCnt;
		readCnt = numGpBuf * NumChannels;

		while (leftToRead > 0) {

			if (leftToRead < numGpBuf) {
				readCnt = leftToRead * NumChannels;
				leftToRead = 0;
			}
			else {
				leftToRead -= numGpBuf;
			}

			numSuccess = fread(gpBuf, BytesPerSample, readCnt, fp);
			if (numSuccess < readCnt) leftToRead = 0;

			if (NumChannels == 1 && BytesPerSample == 1) {
				for (i = 0; i < numSuccess; pointer++, i++) {
					buf[pointer] = ucharp[i];
				}
			}
			else if (NumChannels == 1 && BytesPerSample == 2) {
				memcpy(buf + pointer, shortp, numSuccess);
				pointer += numSuccess;
			}
			else if (NumChannels == 2 && BytesPerSample == 1) {
				for (i = 0; i < numSuccess; pointer++, i += 2) {
					buf[pointer] = ((int)ucharp[i] + (int)ucharp[i + 1]) / 2;
				}
			}
			else if (NumChannels == 2 && BytesPerSample == 2) {
				for (i = 0; i < numSuccess; pointer++, i += 2) {
					buf[pointer] = ((int)shortp[i] + (int)shortp[i + 1]) / 2;
				}
			}
		}

		dataCnt += pointer;
		return pointer;
	}

	int WavFileReader::Read(int *buf, unsigned int leftToRead) {

		unsigned int readCnt, numSuccess, i = 0, pointer = 0;

		if (dataCnt + leftToRead > NumData)leftToRead = NumData - dataCnt;
		readCnt = numGpBuf * NumChannels;

		while (leftToRead > 0) {

			if (leftToRead < numGpBuf) {
				readCnt = leftToRead * NumChannels;
				leftToRead = 0;
			}
			else {
				leftToRead -= numGpBuf;
			}

			numSuccess = fread(gpBuf, BytesPerSample, readCnt, fp);
			if (numSuccess < readCnt) leftToRead = 0;

			if (NumChannels == 1 && BytesPerSample == 1) {
				for (i = 0; i < numSuccess; pointer++, i++) {
					buf[pointer] = ucharp[i];
				}
			}
			else if (NumChannels == 1 && BytesPerSample == 2) {
				for (i = 0; i < numSuccess; pointer++, i++) {
					buf[pointer] = shortp[i];
				}
			}
			else if (NumChannels == 2 && BytesPerSample == 1) {
				for (i = 0; i < numSuccess; pointer++, i += 2) {
					buf[pointer] = ((int)ucharp[i] + (int)ucharp[i + 1]) / 2;
				}
			}
			else if (NumChannels == 2 && BytesPerSample == 2) {
				for (i = 0; i < numSuccess; pointer++, i += 2) {
					buf[pointer] = ((int)shortp[i] + (int)shortp[i + 1]) / 2;
				}
			}
		}

		dataCnt += pointer;
		return pointer;
	}

	int WavFileReader::Read(double *buf, unsigned int leftToRead) {

		unsigned int readCnt, numSuccess, i = 0, pointer = 0;

		if (dataCnt + leftToRead > NumData)leftToRead = NumData - dataCnt;
		readCnt = numGpBuf * NumChannels;

		while (leftToRead > 0) {

			if (leftToRead < numGpBuf) {
				readCnt = leftToRead * NumChannels;
				leftToRead = 0;
			}
			else {
				leftToRead -= numGpBuf;
			}

			numSuccess = fread(gpBuf, BytesPerSample, readCnt, fp);
			if (numSuccess < readCnt) leftToRead = 0;

			if (NumChannels == 1 && BytesPerSample == 1) {
				for (i = 0; i < numSuccess; pointer++, i++) {
					buf[pointer] = ucharp[i];
				}
			}
			else if (NumChannels == 1 && BytesPerSample == 2) {
				for (i = 0; i < numSuccess; pointer++, i++) {
					buf[pointer] = shortp[i];
				}
			}
			else if (NumChannels == 2 && BytesPerSample == 1) {
				for (i = 0; i < numSuccess; pointer++, i += 2) {
					buf[pointer] = ((int)ucharp[i] + (int)ucharp[i + 1]) / 2;
				}
			}
			else if (NumChannels == 2 && BytesPerSample == 2) {
				for (i = 0; i < numSuccess; pointer++, i += 2) {
					buf[pointer] = ((int)shortp[i] + (int)shortp[i + 1]) / 2;
				}
			}
		}

		dataCnt += pointer;
		return pointer;
	}

	int WavFileReader::Read(float *buf, unsigned int leftToRead) {

		unsigned int readCnt, numSuccess, i = 0, pointer = 0;

		if (dataCnt + leftToRead > NumData)leftToRead = NumData - dataCnt;
		readCnt = numGpBuf * NumChannels;

		while (leftToRead > 0) {

			if (leftToRead < numGpBuf) {
				readCnt = leftToRead * NumChannels;
				leftToRead = 0;
			}
			else {
				leftToRead -= numGpBuf;
			}

			numSuccess = fread(gpBuf, BytesPerSample, readCnt, fp);
			if (numSuccess < readCnt) leftToRead = 0;

			if (NumChannels == 1 && BytesPerSample == 1) {
				for (i = 0; i < numSuccess; pointer++, i++) {
					buf[pointer] = ucharp[i];
				}
			}
			else if (NumChannels == 1 && BytesPerSample == 2) {
				for (i = 0; i < numSuccess; pointer++, i++) {
					buf[pointer] = shortp[i];
				}
			}
			else if (NumChannels == 2 && BytesPerSample == 1) {
				for (i = 0; i < numSuccess; pointer++, i += 2) {
					buf[pointer] = ((int)ucharp[i] + (int)ucharp[i + 1]) / 2;
				}
			}
			else if (NumChannels == 2 && BytesPerSample == 2) {
				for (i = 0; i < numSuccess; pointer++, i += 2) {
					buf[pointer] = ((int)shortp[i] + (int)shortp[i + 1]) / 2;
				}
			}
		}

		dataCnt += pointer;
		return pointer;
	}

	int WavFileReader::ReadLR(unsigned char *bufL, unsigned char *bufR, unsigned int leftToRead) {

		unsigned int readCnt, numSuccess, i = 0, pointer = 0;

		if (dataCnt + leftToRead > NumData)leftToRead = NumData - dataCnt;
		readCnt = numGpBuf * NumChannels;

		while (leftToRead > 0) {

			if (leftToRead < numGpBuf) {
				readCnt = leftToRead * NumChannels;
				leftToRead = 0;
			}
			else {
				leftToRead -= numGpBuf;
			}

			numSuccess = fread(gpBuf, BytesPerSample, readCnt, fp);
			if (numSuccess < readCnt) leftToRead = 0;

			if (NumChannels == 1 && BytesPerSample == 1) {
				memcpy(bufL + pointer, ucharp, numSuccess);
				memcpy(bufR + pointer, ucharp, numSuccess);
				pointer += numSuccess;
			}
			else if (NumChannels == 1 && BytesPerSample == 2) {
				for (i = 0; i < numSuccess; pointer++, i++) {
					bufL[pointer] = (unsigned int)((int)shortp[i] + 0x8000) >> 8;
					bufR[pointer] = (unsigned int)((int)shortp[i] + 0x8000) >> 8;
				}
			}
			else if (NumChannels == 2 && BytesPerSample == 1) {
				for (i = 0; i < numSuccess; pointer++, i += 2) {
					bufL[pointer] = ucharp[i];
					bufR[pointer] = ucharp[i + 1];
				}
			}
			else if (NumChannels == 2 && BytesPerSample == 2) {
				for (i = 0; i < numSuccess; pointer++, i += 2) {
					bufL[pointer] = ((int)shortp[i] + 0x8000) >> 8;
					bufR[pointer] = ((int)shortp[i + 1] + 0x8000) >> 8;
				}
			}
		}

		dataCnt += pointer;
		return pointer;
	}

	int WavFileReader::ReadLR(signed short *bufL, signed short *bufR, unsigned int leftToRead) {

		unsigned int readCnt, numSuccess, i = 0, pointer = 0;

		if (dataCnt + leftToRead > NumData)leftToRead = NumData - dataCnt;
		readCnt = numGpBuf * NumChannels;

		while (leftToRead > 0) {

			if (leftToRead < numGpBuf) {
				readCnt = leftToRead * NumChannels;
				leftToRead = 0;
			}
			else {
				leftToRead -= numGpBuf;
			}

			numSuccess = fread(gpBuf, BytesPerSample, readCnt, fp);
			if (numSuccess < readCnt) leftToRead = 0;

			if (NumChannels == 1 && BytesPerSample == 1) {
				for (i = 0; i < numSuccess; pointer++, i++) {
					bufL[pointer] = ucharp[i];
					bufR[pointer] = ucharp[i];
				}
			}
			else if (NumChannels == 1 && BytesPerSample == 2) {
				memcpy(bufL + pointer, shortp, numSuccess);
				memcpy(bufR + pointer, shortp, numSuccess);
				pointer += numSuccess;
			}
			else if (NumChannels == 2 && BytesPerSample == 1) {
				for (i = 0; i < numSuccess; pointer++, i += 2) {
					bufL[pointer] = ucharp[i];
					bufR[pointer] = ucharp[i + 1];
				}
			}
			else if (NumChannels == 2 && BytesPerSample == 2) {
				for (i = 0; i < numSuccess; pointer++, i += 2) {
					bufL[pointer] = shortp[i];
					bufR[pointer] = shortp[i + 1];
				}
			}
		}

		dataCnt += pointer;
		return pointer;
	}

	int WavFileReader::ReadLR(int *bufL, int *bufR, unsigned int leftToRead) {

		unsigned int readCnt, numSuccess, i = 0, pointer = 0;

		if (dataCnt + leftToRead > NumData)leftToRead = NumData - dataCnt;
		readCnt = numGpBuf * NumChannels;

		while (leftToRead > 0) {

			if (leftToRead < numGpBuf) {
				readCnt = leftToRead * NumChannels;
				leftToRead = 0;
			}
			else {
				leftToRead -= numGpBuf;
			}

			numSuccess = fread(gpBuf, BytesPerSample, readCnt, fp);
			if (numSuccess < readCnt) leftToRead = 0;

			if (NumChannels == 1 && BytesPerSample == 1) {
				for (i = 0; i < numSuccess; pointer++, i++) {
					bufL[pointer] = ucharp[i];
					bufR[pointer] = ucharp[i];
				}
			}
			else if (NumChannels == 1 && BytesPerSample == 2) {
				for (i = 0; i < numSuccess; pointer++, i++) {
					bufL[pointer] = shortp[i];
					bufR[pointer] = shortp[i];
				}
			}
			else if (NumChannels == 2 && BytesPerSample == 1) {
				for (i = 0; i < numSuccess; pointer++, i += 2) {
					bufL[pointer] = ucharp[i];
					bufR[pointer] = ucharp[i + 1];
				}
			}
			else if (NumChannels == 2 && BytesPerSample == 2) {
				for (i = 0; i < numSuccess; pointer++, i += 2) {
					bufL[pointer] = shortp[i];
					bufR[pointer] = shortp[i + 1];
				}
			}
		}

		dataCnt += pointer;
		return pointer;
	}

	int WavFileReader::ReadLR(double *bufL, double *bufR, unsigned int leftToRead) {

		unsigned int readCnt, numSuccess, i = 0, pointer = 0;

		if (dataCnt + leftToRead > NumData)leftToRead = NumData - dataCnt;
		readCnt = numGpBuf * NumChannels;

		while (leftToRead > 0) {

			if (leftToRead < numGpBuf) {
				readCnt = leftToRead * NumChannels;
				leftToRead = 0;
			}
			else {
				leftToRead -= numGpBuf;
			}

			numSuccess = fread(gpBuf, BytesPerSample, readCnt, fp);
			if (numSuccess < readCnt) leftToRead = 0;

			if (NumChannels == 1 && BytesPerSample == 1) {
				for (i = 0; i < numSuccess; pointer++, i++) {
					bufL[pointer] = ucharp[i];
					bufR[pointer] = ucharp[i];
				}
			}
			else if (NumChannels == 1 && BytesPerSample == 2) {
				for (i = 0; i < numSuccess; pointer++, i++) {
					bufL[pointer] = shortp[i];
					bufR[pointer] = shortp[i];
				}
			}
			else if (NumChannels == 2 && BytesPerSample == 1) {
				for (i = 0; i < numSuccess; pointer++, i += 2) {
					bufL[pointer] = ucharp[i];
					bufR[pointer] = ucharp[i + 1];
				}
			}
			else if (NumChannels == 2 && BytesPerSample == 2) {
				for (i = 0; i < numSuccess; pointer++, i += 2) {
					bufL[pointer] = shortp[i];
					bufR[pointer] = shortp[i + 1];
				}
			}
		}

		dataCnt += pointer;
		return pointer;
	}

	int WavFileReader::ReadLR(float *bufL, float *bufR, unsigned int leftToRead) {

		unsigned int readCnt, numSuccess, i = 0, pointer = 0;

		if (dataCnt + leftToRead > NumData)leftToRead = NumData - dataCnt;
		readCnt = numGpBuf * NumChannels;

		while (leftToRead > 0) {

			if (leftToRead < numGpBuf) {
				readCnt = leftToRead * NumChannels;
				leftToRead = 0;
			}
			else {
				leftToRead -= numGpBuf;
			}

			numSuccess = fread(gpBuf, BytesPerSample, readCnt, fp);
			if (numSuccess < readCnt) leftToRead = 0;

			if (NumChannels == 1 && BytesPerSample == 1) {
				for (i = 0; i < numSuccess; pointer++, i++) {
					bufL[pointer] = ucharp[i];
					bufR[pointer] = ucharp[i];
				}
			}
			else if (NumChannels == 1 && BytesPerSample == 2) {
				for (i = 0; i < numSuccess; pointer++, i++) {
					bufL[pointer] = shortp[i];
					bufR[pointer] = shortp[i];
				}
			}
			else if (NumChannels == 2 && BytesPerSample == 1) {
				for (i = 0; i < numSuccess; pointer++, i += 2) {
					bufL[pointer] = ucharp[i];
					bufR[pointer] = ucharp[i + 1];
				}
			}
			else if (NumChannels == 2 && BytesPerSample == 2) {
				for (i = 0; i < numSuccess; pointer++, i += 2) {
					bufL[pointer] = shortp[i];
					bufR[pointer] = shortp[i + 1];
				}
			}
		}

		dataCnt += pointer;
		return pointer;
	}

	int WavFileReader::Seek(long offset, int origin) {
		if ((long)dataCnt < (-1) * offset) {//changed the type of dataCnt from uint to long, so I removed the casting dataCnt to long
			fseek(fp, (-1) * dataCnt * BlockAlign, origin);
			dataCnt = 0;
			return 1;
		}
		if((long)dataCnt + offset > NumData){
			fseek(fp, (NumData-dataCnt)*BlockAlign, origin);
			dataCnt = NumData;
			return 1;
		}
		dataCnt += offset;
		return fseek(fp, BlockAlign*offset, origin);
	}

	unsigned int WavFileReader::Tell() {
		return (long)dataCnt;
	}


}


//MIT License

//Copyright(c) 2019 GoldSakana
//https://twitter.com/goldsakana

//Permission is hereby granted, free of charge, to any person obtaining a copy
//of this software and associated documentation files(the "Software"), to deal
//in the Software without restriction, including without limitation the rights
//to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//copies of the Software, and to permit persons to whom the Software is
//furnished to do so, subject to the following conditions :

//The above copyright notice and this permission notice shall be included in all
//copies or substantial portions of the Software.

//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
//AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//SOFTWARE.
