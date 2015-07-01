// compile with: g++ -std=c++11 -O3 `root-config --cflags --glibs` -lSpectrum merge_histograms.cpp -o merge_histograms

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <vector>
#include <list>
#include <dirent.h>
#include <unistd.h>  // getopt
#include <getopt.h>  // getopt_long
#include <errno.h>
#include <libgen.h>  // basename, dirname
#include <ctype.h>  // isdigit
#include <limits.h>
//#include <sys/types.h>

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
              << indent << "histograms stored in the given files"
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

const char* join_path(const char* path1, const char* path2, const char* path_sep = "/")
{
	char* path = new char[PATH_MAX+1];
	strcpy(path, path1);
	strcat(path, path_sep);
	strcat(path, path2);

	return path;
}

void remove_trailing_slash(char* path)
{
	std::string pathname(path);
	while (!pathname.empty() && pathname.back() == '/')
		pathname.pop_back();

	path = const_cast<char*>(pathname.c_str());
}

void add_trailing_slash(char* path)
{
	std::string pathname(path);
	if (!pathname.empty() && pathname.back() != '/')
		pathname += '/';

	path = const_cast<char*>(pathname.c_str());
}

/*char* get_basename(char* path)
{
    char *base = strrchr(path, '/');
    return base ? base+1 : path;
}*/

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

/*	char* _path = realpath(path, NULL);

	if (!_path) {
		fprintf(stderr, "The path '%s' could not be resolved!\n", path);
		exit(EXIT_FAILURE);
	}

	free(_path);*/

	return NULL;
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
	std::vector<const char*> histograms;
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
					histograms.push_back(optarg);
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
				//printf("Use config file: %s\n", optarg);
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
				//printf("Use directory: %s\n", optarg);
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
			histograms.push_back(argv[i]);

	if (!input[0] && !path[0]) {
		fprintf(stderr, "You've specified neither a file nor a directory as input!\n");
		return EXIT_FAILURE;
	} else if (input[0] && path[0]) {
		fprintf(stderr, "Use either -i/--input-file or -d/--directory, not both!\n");
		return EXIT_FAILURE;
	}

	if (merge_all)
		std::cout << "All histograms from the files will be read in and merged" << std::endl;
	else {
		std::cout << "The following " << histograms.size() << " histograms will be considered:" << std::endl;
		for (auto && str : histograms)
			printf("   %s\n", str);
	}
	putchar('\n');

	if (verbose_flag)
		printf("verbose_flag is set\n");

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

	for (auto it : files)
		std::cout << it << std::endl;

	return EXIT_SUCCESS;
}

