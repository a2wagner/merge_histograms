// compile with: g++ -std=c++11 -O3 `root-config --cflags --glibs` -lSpectrum merge_histograms.cpp -o merge_histograms

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include <vector>
#include <list>
#include <algorithm>
#include <dirent.h>
#include <unistd.h>  // getopt
#include <getopt.h>  // getopt_long
#include <errno.h>
#include <libgen.h>  // basename, dirname
#include <ctype.h>  // isdigit
#include <limits.h>
//#include <sys/types.h>

#include "TFile.h"
#include "TH1.h"
#include "TH2.h"
#include "TH3.h"

/* Flag set by `--verbose'. */
static int verbose_flag;

/* specify the expected options */
static struct option long_options[] = {
	/* options set a flag */
	{"verbose", no_argument, &verbose_flag, 1},
	// to also use the short form one has to use {"verbose", no_argument, 0, 'v'} and handle it in the switch
	/* options don't set a flag, distinguish them by their indices */
	{"help", no_argument, 0, 'h'},
	{"output", required_argument, 0, 'o'},
	{"plots", required_argument, 0, 'p'},
	{"directory", required_argument, 0, 'd'},
	{"input-file", required_argument, 0, 'i'},
	{0, 0, 0, 0}
};

void print_help(const char* name)
{
    const char* indent = "\t\t\t\t";
    std::cerr << "Usage: " << name << " <option(s)> -p HISTOGRAMS\n"
              << "Options:\n"
              << "\t-h, --help\t\tShow this help message\n"
              << "\t-i, --input-file FILE\tFile containing a list of files which\n"
              << indent << "should be used\n"
              << "\t-d, --directory INPUT-DIR   Specify a directory name to scan\n"
              << indent << "recursively for files\n"
              << "\t-o, --output FILENAME\tFile name where the merged histograms\n"
              << indent << "should be stored\n"
              << "\t-p, --plots HIST_1 [HIST_2, ..., HIST_N]   Name(s) of the\n"
              << indent << "histogram(s) which should be merged\n"
              << indent << "from each input file\n"
              << indent << "Use the keyword 'all' to merge all\n"
              << indent << "histograms stored in the given files\n"
              << "\t--verbose\t\tPrint additional information"
              << std::endl;
}

static unsigned int is_udec(char* const str)
{
	char *p = &str[0];

	while (*p)
		if (!isdigit(*p++))
			return 0;

	return str[0];
}

// check if a given char array is a decimal
static unsigned int is_decimal(char* const str)
{
	return (is_udec(&str[(str[0] == '-') || (str[0] == '+')]));
}

// method for a non case sensitive comparison of strings
bool cmp_nocase(std::string first, std::string second)
{
	unsigned int i = 0;
	while ((i < first.length()) && (i < second.length())) {
		if (tolower(first[i]) < tolower(second[i])) return true;
		else if (tolower(first[i]) > tolower(second[i])) return false;
		++i;
	}
	if (first.length() > second.length()) return true;
	else return false;
}

void remove_trailing_slash(char* path)
{
	std::string pathname(path);
	while (!pathname.empty() && pathname.back() == '/')
		pathname.pop_back();

	strcpy(path, pathname.c_str());
}

void remove_trailing_slash(std::string& path)
{
	while (!path.empty() && path.back() == '/')
		path.pop_back();
}

void add_trailing_slash(char* path)
{
	std::string pathname(path);
	if (!pathname.empty() && pathname.back() != '/')
		pathname += '/';

	strcpy(path, pathname.c_str());
}

void add_trailing_slash(std::string& path)
{
	if (!path.empty() && path.back() != '/')
		path += '/';
}

const char* join_path(const char* path1, const char* path2, const char* path_sep = "/")
{
	char* path = new char[PATH_MAX+1];
	strcpy(path, path1);
	strcat(path, path_sep);
	strcat(path, path2);

	return path;
}

const char* join_path(const std::string path1, const std::string path2, const std::string path_sep = "/")
{
	return (path1 + path_sep + path2).c_str();
}

const std::string join_path_str(const std::string path1, const std::string path2)
{
	std::string path(path1);
	add_trailing_slash(path);
	return path + path2;
}

std::string get_base_name(const std::string& str,
                          const std::string& path_sep = "/\\")
{
	const size_t end = str.find_last_of(path_sep);
	if (end == std::string::npos)
		return str;  // no path given, return string

	return str.substr(0, end);
}

// trim whitespaced at the beginning and the end of a string
std::string trim(const std::string& str,
                 const std::string& whitespace = " \t")
{
	const auto begin = str.find_first_not_of(whitespace);
	if (begin == std::string::npos)
		return "";  // no content

	const auto end = str.find_last_not_of(whitespace);
	const auto range = end - begin + 1;

	return str.substr(begin, range);
}

// first trim the string, then reduce whitespaces between substrings to 'fill'
std::string reduce(const std::string& str,
                   const std::string& fill = " ",
                   const std::string& whitespace = " \t")
{
	// trim first
	auto result = trim(str, whitespace);

	// replace sub ranges
	auto begin_space = result.find_first_of(whitespace);
	while (begin_space != std::string::npos)
	{
		const auto end_space = result.find_first_not_of(whitespace, begin_space);
		const auto range = end_space - begin_space;

		result.replace(begin_space, range, fill);

		const auto new_start = begin_space + fill.length();
		begin_space = result.find_first_of(whitespace, new_start);
	}

	return result;
}

// recursively get all files from a given directory path
void list_files(const char* path, std::list<std::string>& file_list)
{
	DIR *dir;
	struct dirent *ent;

	if (dir = opendir(path)) {
		while (ent = readdir(dir))  // loop over dir as long as ent gets a pointer assigned
			if (ent->d_type == DT_DIR)  // is directory?
				if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
					continue;  // skip . and .. dirs
				else
					list_files(join_path(path, ent->d_name), file_list);
					// in case of strings this can be easily written as list_files(path + '/' + std::string(ent->d_name)), file_list);
			else
				file_list.push_back(join_path(path, ent->d_name));

		closedir(dir);
	} else {
		fprintf(stderr, "The directory '%s' could not be read!\n", path);
		exit(EXIT_FAILURE);
	}

	return;
}

// remove all entries from the list which don't contain the given pattern
void filter_list(std::list<std::string>& list, const char* pattern)
{
	std::list<std::string>::iterator it = list.begin();
	while (it != list.end())
		if (!strstr((*it).c_str(), pattern))
			it = list.erase(it);
		else
			it++;
}

// get the real path of the given char array if it contains any special chars
const char* get_real_path(const char* path)
{
	DIR *dir;

	// check if the directory could be opened
	dir = opendir(path);
	if (dir) {
		closedir(dir);
		return path;
	}

	char* _path = (char*)malloc(PATH_MAX+1);  // allocate memory for new path
	// if not, try to resolve the path if it contains a tilde
	if (*path == '~') {
		// build the new path by replacing the home directory
		strcpy(_path, getenv("HOME"));
		strcat(_path, path+1);

		// check if the directory could be opened
		dir = opendir(_path);
		if (dir) {  // check if we can open this new path
			closedir(dir);
			return _path;
		} else {  // if not, let's assume it might be a file and get the dirname of it
			const char* new_path = dirname(_path);
			return new_path;
		}
	}

	// if the above doesn't work, let's give realpath() a try to do it
	// it should resolve the path (~, symlinks, ...), but often doesn't work as expected...
	realpath(path, _path);
	// get the dirname in case a file is appended or a tilde in some strange cases
	const char* new_path = dirname(_path);

	// check if the directory could be opened
	dir = opendir(new_path);
	if (dir) {
		closedir(dir);
		return new_path;
	}

	return NULL;
}

// get the directory where a program (with a specific pid) is located
int get_program_directory(char* path,
                          size_t size = PATH_MAX,
                          pid_t pid = getpid())
{
	char sz_tmp[32];
	char buf[size];
	sprintf(sz_tmp, "/proc/%d/exe", pid);
	ssize_t len = readlink(sz_tmp, buf, sizeof(buf)-1);
	if (len == -1)
		return len;
	buf[len] = '\0';
	// find last occurence of '/' to get obtain the path
	char* ch = strrchr(buf, '/');
	// set end marker to the new position
	buf[ch-buf] = '\0';
	strcpy(path, buf);
	return 0;
}

// get the directory where the current program is located
std::string get_selfpath()
{
	char buf[PATH_MAX];
	ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf)-1);
	if (len != -1) {
		buf[len] = '\0';
		std::string str(buf);
		// remove program name from path
		str = get_base_name(str);
		return str;
	} else {
		std::cerr << "Overflow by reading program path (off-by-one underflow)"
		<< std::endl;
		exit(EXIT_FAILURE);
	}
}

// check if a file exists by opening and closing it
int check_file(const char* name)
{
	FILE *f = NULL;
	f = fopen(name, "r");
	if (!f)
		return 0;
	fclose(f);
	return 1;
}

int main(int argc, char** argv)
{
	// the extension of the files which should be used
	const char ext[8] = ".root";
	// variables needed for the argument parsing
	opterr = 0;  // use own error messages instead of getopt buildin ones
	int opt;
	int option_index;
	// other variables to store the needed information
	FILE *f = NULL;
	DIR *d = NULL;
	char input[PATH_MAX+1] = "";
	char output[PATH_MAX+1] = "";
	char path[PATH_MAX+1] = "";
	bool merge_all = false;
	std::vector<std::string> histograms;
	std::list<std::string> files;

	/*
	 * POSIX: if the first letter of the optstring is a colon, a colon is returned if an argument is missing, otherwise a questionmark
	 * A -- will terminate the while loop, './program -- abc' won't process the other arguments
	 */
	while ((opt = getopt_long(argc, argv, "ho:p:d:i:", long_options, &option_index)) != -1) {
		switch (opt) {
			case 0:
				/* If this option sets a flag, do nothing else now. */
				if (long_options[option_index].flag != 0)
					break;
			case 'p':
				if (!strcmp(optarg, "all"))
					merge_all = true;
				else
					histograms.push_back(std::string(optarg));
				break;
			case 'o':
				strcpy(output, optarg);
				break;
			case 'i':
				if (input[0]) {
					fprintf(stderr, "Invalid parameters: only one file allowed as an argument\n");
					return EXIT_FAILURE;
				}
				f = fopen(optarg, "r");
				if (!f) {
					fprintf(stderr, "Error opening file %s: %s\n", optarg, strerror(errno));
					return EXIT_FAILURE;
				}
				strcpy(input, optarg);
				fclose(f);
				f = NULL;  // set the pointer to NULL, otherwise a check for it will return true and closing the already closed file will cause undefined behaviour --> probably segfault
				break;
			case 'd':
				if (path[0]) {
					fprintf(stderr, "Invalid parameters: only one directory allowed as an argument\n");
					return EXIT_FAILURE;
				}
				// check if the directory could be opened
				d = opendir(optarg);
				if (!d) {
					fprintf(stderr, "Error opening the directory %s: %s\n", optarg, strerror(errno));
					return EXIT_FAILURE;
				}
				strcpy(path, optarg);
				remove_trailing_slash(path);
				closedir(d);
				d = NULL;
				break;
			case 'h':
				print_help(argv[0]);
				return EXIT_SUCCESS;
			case ':':
				// option argument is missing
				fprintf(stderr, "%s: option '-%c' requires an argument\n", argv[0], optopt);
				return EXIT_FAILURE;
			case '?':
			default:
				fprintf(stderr, "%s: option '-%c' is invalied\n", argv[0], optopt);
				print_help(argv[0]);
				return EXIT_FAILURE;
		}
	}

	if (!merge_all)
		for (int i = optind; i < argc; i++)
			histograms.push_back(std::string(argv[i]));

	if (!input[0] && !path[0]) {
		fprintf(stderr, "You've specified neither a file nor a directory as input!\n");
		return EXIT_FAILURE;
	} else if (input[0] && path[0]) {
		fprintf(stderr, "Use either -i/--input-file or -d/--directory, not both!\n");
		return EXIT_FAILURE;
	}

	if (verbose_flag)
		std::cout << "verbose flag is set\nAdditional information will be printed" << std::endl;

	// if path is specified, read in all root files which are stored in this directory
	if (path[0]) {
		// get a list of all files
		list_files(path, files);
		if (files.empty()) {
			fprintf(stderr, "Directory '%s' doesn't contain any files!\n", path);
			return EXIT_FAILURE;
		}

		// filter the list for root files
		filter_list(files, ext);
		if (files.empty()) {
			fprintf(stderr, "Directory '%s' doesn't contain any %s-files!\n", path, ext);
			return EXIT_FAILURE;
		}
	} else if (input[0]) {  // else input has to be specified, read the list and filter if necessary
		std::ifstream in(input);
		std::string line;
		// paths to find the given root files in case of relative paths
		char cwd[PATH_MAX];
		getcwd(cwd, PATH_MAX-1);
		std::string program_path = get_selfpath();
		while (std::getline(in, line)) {
			line = trim(line);
			// skip empty lines or lines which start with a hash
			if (line.empty() || line.find("#") == 0)
				continue;
			// check if the string contains the desired extension, skip if not
			if (!strstr(line.c_str(), ext))
				continue;
			// try to find the files
			if (check_file(line.c_str()))
				files.push_back(line);
			else if (check_file(join_path(cwd, line.c_str())))
				files.push_back(std::string(join_path(cwd, line.c_str())));
			else if (check_file(join_path(program_path, line)))
				files.push_back(std::string(join_path_str(program_path, line)));
			else
				printf("WARNING: Couldn't find file '%s', skip it\n", line.c_str());
		}
		in.close();
	} else {  // this case should never happen
		std::cerr << "No source to obtain list of files existing, this shouldn't happen!" << std::endl;
		return EXIT_FAILURE;
	}

	std::cout << "The following files will be merged:" << std::endl;
	for (auto it : files)
		std::cout << it << std::endl;
	putchar('\n');

	if (merge_all && verbose_flag)
		std::cout << "All histograms from the files will be read in and merged" << std::endl;

	/* start of the ROOT related code */

	TFile* file;
	TDirectory* dir;
	//TIterator iter;
	char dir_name[128];
	std::vector<TH1*> merged_histograms;
	// if all histograms should be merged, open the first file to get a list of all histograms
	if (merge_all) {
		file = TFile::Open(files.front().c_str());
		if (!file->IsOpen()) {
			std::cerr << "Unable to open file '" << file->GetName() << "'. Will terminate." << std::endl;
			return EXIT_FAILURE;
		}
		// check if there is more than one directory in the file
		if (file->GetListOfKeys()->GetSize() > 1)
			std::cout << "Found more than one directory in file " << file->GetName() << std::endl
				<< "Will only use the first directory in all files" << std::endl;
		else if (!file->GetListOfKeys()->GetSize()) {
			std::cerr << "Found no directory in file '" << file->GetName() << "'. Will terminate." << std::endl;
			file->Close();
			return EXIT_FAILURE;
		}
		strcpy(dir_name, file->GetListOfKeys()->First()->GetName());
		dir = file->GetDirectory(dir_name);
		//iter = dir->GetListOfKeys()->MakeIterator();
		TIter iter(dir->GetListOfKeys());
		std::for_each(iter.Begin(), TIter::End(), [&histograms](TObject* o){ histograms.push_back(std::string(o->GetName())); });
		if (verbose_flag) {
			auto list_infos = [](TObject* o)
				{
					std::cout << "Name: "
						<< o->GetName()
						<< ",\tTitle: "
						<< o->GetTitle()
						<< std::endl;
				};
			std::cout << "The following histograms are stored in the file:" << std::endl;
			std::for_each(iter.Begin(), TIter::End(), list_infos);
			putchar('\n');
		}
		file->Close();
	}

	std::cout << "The following " << histograms.size() << " histograms will be considered:" << std::endl;
	for (auto && str : histograms)
		printf("   %s\n", str.c_str());

	for (auto it : files) {
		file = TFile::Open(it.c_str());
		if (!file->IsOpen()) {
			std::cerr << "Unable to open file '" << file->GetName() << "'. It will be skipped." << std::endl;
			continue;
		}
		if (!file->GetListOfKeys()->GetSize()) {
			std::cerr << "Found no directory in file '" << file->GetName() << "'. Skip this file." << std::endl;
			file->Close();
			continue;
		}
		strcpy(dir_name, file->GetListOfKeys()->First()->GetName());
		dir = file->GetDirectory(dir_name);
		if (merged_histograms.empty())
			for (auto && name : histograms) {
				TH1* h = dynamic_cast<TH1*>(dir->Get(name.c_str()));
				h->SetDirectory(0);  // revoke gDirectory object ownership that this histogram won't get deleted when the file or directory is closed
				merged_histograms.push_back(h);
			}
		else
			for (auto hist : merged_histograms) {
				TH1* h = dynamic_cast<TH1*>(dir->Get(hist->GetName()));
				if (!h) {
					std::cerr << "Histogram " << hist->GetName() << "not found in file " << file->GetName() << ". Skipt it." << std::endl;
					continue;
				}
				hist->Add(h);
			}
		file->Close();
	}

	file = TFile::Open(output, "RECREATE");
	for (auto hist : merged_histograms)
		hist->Write();
	file->Close();

	for (auto hist : merged_histograms)
		delete hist;

	return EXIT_SUCCESS;
}

