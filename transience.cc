#define VERSION "0.5"

#include <stdio.h>
#include <stdlib.h>
#include <sndfile.h>
#include <sys/stat.h>
#include <vector>
#include <cstring>
#include <string>
#include <unistd.h>
#include "transient.h"

std::vector<transient> generateTransients_ThresholdVolume(double minVolume, int len, SNDFILE *audioFile, SF_INFO *info);
bool checkValidSettings(char path[], double minVolume, double len, int fflag, std::vector<transient> &transients);
void exportTransients(char path[], std::vector<transient> &transients, int normalize);

int main(int argc, char **argv) {
	 /*TODO:
	 *  - Specify file output formats
	 *  - Add new detection methods
	 *  	- By spectral characteristics with FFT
	 *  	- Set threshold based on desired number of transients */

	int hflag = 0;
	int vflag = 0;
	int nflag = 0;
	int fflag = 0;
	int dflag = 0;
	char *lvalue = NULL;
	char *tvalue = NULL;
	int index;
	int arg;

	opterr = 0;

	while((arg = getopt(argc, argv, "hvnfdl:t:")) != -1) {
		switch(arg) {
			case 'h':
				hflag = 1; break;
			case 'v':
				vflag = 1; break;
			case 'n':
				nflag = 1; break;
			case 'f':
				fflag = 1; break;
			case 'd':
				dflag = 1; break;
			case 'l':
				lvalue = optarg; break;
			case 't':
				tvalue = optarg; break;
			case '?':
				if(optopt == 'l' || optopt == 't') fprintf(stderr, "Option -%c requires an argument. Try '%s -h'.\n", optopt, argv[0]);
				else if(isprint(optopt)) fprintf(stderr, "Unknown option -%c. Try '%s -h'.\n", optopt, argv[0]);
				else fprintf(stderr, "Unknown option character '\\x%x'. Try '%s -h'.\n", optopt, argv[0]);
				return 1;
			default:
				abort();
		}
	}

	if(hflag || vflag) {
		if(vflag)
			printf("Transience %s - developed by Anton Lazarev\n", VERSION);
		if(hflag) {
			printf("%s [-hvnfd] FILENAME[s...] -l LENGTH -t THRESHOLD\n\n", argv[0]);
			printf("	-h		Display this help information\n");
			printf("	-v		Display program version\n\n");
			printf("	-l		Set length of output transients in seconds\n");
			printf("	-t		Set detection threshold volume (0-1]\n\n");
			printf("	-n		Normalize transients\n");
			printf("	-f		Specify length in frames\n");
			printf("	-d		Dry run (do not export)\n\n");
			printf("Transience is a small tool designed to programatically slice audio recordings into short percussive elements.\n");
			printf("Supported audio formats include WAV, AIFF, FLAC, and others.\n");
		}
		return 0;
	}

	if(lvalue && tvalue) {
		double len = std::stod(lvalue, NULL);
		double threshold = std::stod(tvalue, NULL);

		for (index = optind; index < argc; index++) {
			char **path = &argv[index];
			std::vector<transient> transients;

			if(!checkValidSettings(*path, threshold, len, fflag, transients)) {
				printf("No transients could be generated using the specified settings.\n");
			}
			else if(!dflag) {
				exportTransients(*path, transients, nflag);
			}
			else {
				printf("%lu transient%s detected.\n", transients.size(), (transients.size() == 1) ? " was" : "s were");
			}
		}
	}
	else {
		if(!lvalue) printf("Error: Required flag -l is missing.\n");
		if(!tvalue) printf("Error: Required flag -t is missing.\n");
		printf("Try '%s -h' for more information.\n", argv[0]);
	}
}

bool checkValidSettings(char path[], double minVolume, double len, int fflag, std::vector<transient> &transients) {
	//check that path exists
	struct stat buf;
	if(stat(path, &buf)) {
		printf("The specified path does not exist.\n");
		return false;
	}

	//check that path is a valid audio file
	SF_INFO info;
	info.format = 0;
	SNDFILE *audioFile = sf_open(path, SFM_READ, &info);
	if(sf_error(audioFile)) {
		printf("The specified path is not a valid audio file.\n");
		return false;
	}

	//convert length to frames if it is not already in frames
	if(!fflag) len = len*info.samplerate;

	//If at least one transient can be generated, return all transients.
	transients = generateTransients_ThresholdVolume(minVolume, (int) len, audioFile, &info);
	sf_close(audioFile);
	return (transients.size() > 0);
}

std::vector<transient> generateTransients_ThresholdVolume(double minVolume, int len, SNDFILE *audioFile, SF_INFO *info) {
	double* data = new double[info->frames*info->channels];
	sf_readf_double(audioFile, data, info->frames);
	std::vector<transient> transients;
	for(int x = 0; x < info->frames*info->channels; x++) {
		if(data[x] > minVolume || data[x] < -minVolume) {
			if (x+len < info->frames*info->channels) transients.push_back(transient(x,x+len*info->channels));
			else transients.push_back(transient(x,info->frames*info->channels));
			x = x + len*info->channels;
		}
	}
	delete[] data;
	return transients;
}

void exportTransients(char path[], std::vector<transient> &transients, int normalize) {
	SF_INFO src_info;
	src_info.format = 0;
	SNDFILE *source = sf_open(path, SFM_READ, &src_info);
	double* data = new double[src_info.frames*src_info.channels];
	sf_readf_double(source, data, src_info.frames);

	char * extension = strrchr(path, '.');
	char filename[strlen(path)-strlen(extension)];
	strncpy(filename, path, strlen(path)-strlen(extension));
	filename[strlen(path)-strlen(extension)] = '\0';

	for(unsigned int x = 0; x < transients.size(); x++) {
		std::string xpath = (std::string) filename + "_transient_" + std::to_string(x) + extension;
		int len = transients[x].end - transients[x].start;

		double* t_data = new double[len];

		//create transient files
		SF_INFO t_info;
		t_info.samplerate = src_info.samplerate;
		t_info.channels = src_info.channels;
		t_info.format = src_info.format;
		SNDFILE *segment = sf_open(xpath.c_str(), SFM_WRITE, &t_info);

		double max_val = 0;
		for(int f = 0; f < len; f++) {
			t_data[f] = data[transients[x].start+f];
			if(normalize) {
				if(t_data[f] > max_val) max_val = t_data[f];
			}
		}

		if(normalize) {
			for(int n = 0; n < len; n++) {
				t_data[n] = t_data[n]/max_val;
			}
		}

		sf_writef_double(segment, t_data, len);

		delete[] t_data;
		sf_close(segment);
	}
	printf("%lu transient%s been written to '%s_transient_*%s'.\n", transients.size(), (transients.size()==1) ? " has" : "s have", filename, extension);
	delete[] data;
	sf_close(source);
}
