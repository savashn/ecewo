#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#include <conio.h> // _getch()
#define PATH_SEPARATOR "\\"
#define mkdir(path, mode) _mkdir(path)
#define access(path, mode) _access(path, mode)
#define F_OK 0
#define popen _popen
#define pclose _pclose
#define system_command(cmd) system(cmd)
#else
#include <unistd.h>
#include <sys/wait.h>
#include <unistd.h>  // read(), STDIN_FILENO
#include <termios.h> // tcgetattr(), tcsetattr()
#include <dirent.h>  // for cmake configuration
#define PATH_SEPARATOR "/"
#define system_command(cmd) system(cmd)

// POSIX terminal raw mode
static struct termios orig_term;
void enable_raw_mode()
{
    struct termios raw;
    if (tcgetattr(STDIN_FILENO, &orig_term) == -1)
        return;
    raw = orig_term;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}
void disable_raw_mode()
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_term);
}
#endif

#define REPO_URL "https://github.com/savashn/ecewo"

// Plugin URLs
#define CJSON_C_URL "https://raw.githubusercontent.com/savashn/ecewo/main/vendors/cJSON.c"
#define CJSON_H_URL "https://raw.githubusercontent.com/savashn/ecewo/main/vendors/cJSON.h"
#define DOTENV_C_URL "https://raw.githubusercontent.com/savashn/ecewo/main/vendors/dotenv.c"
#define DOTENV_H_URL "https://raw.githubusercontent.com/savashn/ecewo/main/vendors/dotenv.h"
#define SQLITE_C_URL "https://raw.githubusercontent.com/savashn/ecewo/main/vendors/sqlite3.c"
#define SQLITE_H_URL "https://raw.githubusercontent.com/savashn/ecewo/main/vendors/sqlite3.h"
#define COOKIE_C_URL "https://raw.githubusercontent.com/savashn/ecewo/main/plugins/cookie.c"
#define COOKIE_H_URL "https://raw.githubusercontent.com/savashn/ecewo/main/plugins/cookie.h"
#define SESSION_C_URL "https://raw.githubusercontent.com/savashn/ecewo/main/plugins/session.c"
#define SESSION_H_URL "https://raw.githubusercontent.com/savashn/ecewo/main/plugins/session.h"
#define ASYNC_C_URL "https://raw.githubusercontent.com/savashn/ecewo/main/plugins/async.c"
#define ASYNC_H_URL "https://raw.githubusercontent.com/savashn/ecewo/main/plugins/async.h"

typedef struct
{
    const char *name;
    const char *folder;
    const char *c_url;
    const char *h_url;
    int selected;
} Plugin;

static Plugin plugins[] = {
    {"cJSON", "vendors", CJSON_C_URL, CJSON_H_URL, 0},
    {"dotenv", "vendors", DOTENV_C_URL, DOTENV_H_URL, 0},
    {"sqlite3", "vendors", SQLITE_C_URL, SQLITE_H_URL, 0},
    {"cookie", "plugins", COOKIE_C_URL, COOKIE_H_URL, 0},
    {"session", "plugins", SESSION_C_URL, SESSION_H_URL, 0},
    {"async", "plugins", ASYNC_C_URL, ASYNC_H_URL, 0},
    {"cbor", NULL, NULL, NULL, 0},
    {"l8w8jwt", NULL, NULL, NULL, 0},
};

static const int plugin_count = sizeof(plugins) / sizeof(Plugin);

// Command flags
typedef struct
{
    int run;
    int rebuild;
    int update;
    int migrate;
    int install;
    int create;
    int help;
    int cjson;
    int dotenv;
    int sqlite;
    int cookie;
    int session;
    int async_plugin;
    int cbor;
    int l8w8jwt;
} flags_t;

//
//
// <---- UTILITY FUNCTIONS ---->
//
//

// String builder struct to create dynamic strings
typedef struct
{
    char *data;
    size_t size;
    size_t capacity;
} StringBuilder;

StringBuilder *sb_create()
{
    StringBuilder *sb = malloc(sizeof(StringBuilder));
    sb->capacity = 256;
    sb->size = 0;
    sb->data = malloc(sb->capacity);
    sb->data[0] = '\0';
    return sb;
}

void sb_append(StringBuilder *sb, const char *str)
{
    size_t len = strlen(str);
    while (sb->size + len + 1 > sb->capacity)
    {
        sb->capacity *= 2;
        sb->data = realloc(sb->data, sb->capacity);
    }
    strcat(sb->data, str);
    sb->size += len;
}

void sb_free(StringBuilder *sb)
{
    free(sb->data);
    free(sb);
}

int file_exists(const char *path)
{
    return access(path, F_OK) == 0;
}

int directory_exists(const char *path)
{
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

int create_directory(const char *path)
{
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);

    // Separate according to PATH_SEPARATOR
    for (size_t i = 1; i < len; i++)
    {
        if (tmp[i] == '/' || tmp[i] == '\\')
        {
            tmp[i] = '\0';
            if (access(tmp, F_OK) != 0)
            {
                if (mkdir(tmp, 0755) != 0)
                    return -1;
            }
            tmp[i] = PATH_SEPARATOR[0];
        }
    }

    // Create the last directory
    if (access(tmp, F_OK) != 0)
    {
        if (mkdir(tmp, 0755) != 0)
            return -1;
    }

    return 0;
}

int remove_directory(const char *path)
{
    char command[1024];
#ifdef _WIN32
    snprintf(command, sizeof(command), "rmdir /s /q \"%s\" 2>nul", path);
#else
    snprintf(command, sizeof(command), "rm -rf \"%s\"", path);
#endif
    return system_command(command);
}

int execute_command(const char *command)
{
    printf("Executing: %s\n", command);
    return system_command(command);
}

int download_file(const char *url, const char *output_path)
{
    char command[1024];
    snprintf(command, sizeof(command), "curl -o \"%s\" \"%s\"", output_path, url);
    return execute_command(command);
}

// Check if string contains substring
int contains_string(const char *haystack, const char *needle)
{
    return strstr(haystack, needle) != NULL;
}

// Read file content
char *read_file(const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (!file)
    {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *content = malloc(length + 1);
    if (!content)
    {
        fclose(file);
        return NULL;
    }

    fread(content, 1, length, file);
    content[length] = '\0';
    fclose(file);

    return content;
}

// Write file content
int write_file(const char *filename, const char *content)
{
    FILE *file = fopen(filename, "w");
    if (!file)
    {
        return -1;
    }

    fprintf(file, "%s", content);
    fclose(file);
    return 0;
}

//
//
// <---- SELECT MENU ---->
//
//

// Clear the console screen
void clear_screen()
{
#ifdef _WIN32
    system("cls");
#else
    printf("\033[2J\033[H");
#endif
}

// Draw menu for selecting plugin
void draw_menu(int current)
{
    clear_screen();
    printf("Select plugins: (space to toggle, arrow keys to move, enter to confirm)\n\n");
    for (int i = 0; i < plugin_count; i++)
    {
        printf(" %c [%c] %s\n",
               i == current ? '>' : ' ',
               plugins[i].selected ? 'x' : ' ',
               plugins[i].name);
    }
}

// Read the keyboard
int read_key()
{
#ifdef _WIN32
    int ch = _getch();
    if (ch == 0 || ch == 224)
    {
        int dir = _getch();
        return 1000 + dir; // Add offset
    }
    return ch;
#else
    char c;
    if (read(STDIN_FILENO, &c, 1) != 1)
        return -1;
    if (c == '\x1b')
    { // ESC
        char seq[2];
        if (read(STDIN_FILENO, &seq[0], 1) != 1)
            return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1)
            return '\x1b';

        if (seq[0] == '[')
        {
            if (seq[1] == 'A')
                return 1000 + 72; // UP
            if (seq[1] == 'B')
                return 1000 + 80; // DOWN
            if (seq[1] == 'C')
                return 1000 + 77; // RIGHT
            if (seq[1] == 'D')
                return 1000 + 75; // LEFT
        }

        return '\x1b';
    }
    return c;
#endif
}

// Select plugin in the creating project step
void interactive_select()
{
    int current = 0;
#ifndef _WIN32
    enable_raw_mode();
#endif

    while (1)
    {
        draw_menu(current);
        int c = read_key();

        if (c == ' ')
        {
            plugins[current].selected = !plugins[current].selected;
        }
        else if (c == '\r' || c == '\n')
        {
            break;
        }
        else if (c == 'w' && current > 0)
        {
            current--;
        }
        else if (c == 's' && current < plugin_count - 1)
        {
            current++;
        }
        else if (c == 1000 + 72 && current > 0) // UP
        {
            current--;
        }
        else if (c == 1000 + 80 && current < plugin_count - 1) // DOWN
        {
            current++;
        }
    }

#ifndef _WIN32
    disable_raw_mode();
#endif
    printf("\n");
}

//
//
// <---- PLUGINS AND CMAKE UPDATING ---->
//
//

// Platform-agnostic path building
void build_plugin_path(char *buffer, size_t buffer_size, const char *plugin_name, const char *extension, const char *folder)
{
    snprintf(buffer, buffer_size,
             "core%s%s%s%s.%s",
             PATH_SEPARATOR,
             folder,
             PATH_SEPARATOR,
             plugin_name,
             extension);
}

// Cross-platform directory listing
#ifdef _WIN32
// Internal helper function for recursion
char *find_c_files_internal(const char *current_dir, const char *original_base_dir, int use_cmake_prefix);

char *find_c_files(const char *base_dir, int use_cmake_prefix)
{
    return find_c_files_internal(base_dir, base_dir, use_cmake_prefix);
}

char *find_c_files_internal(const char *current_dir, const char *original_base_dir, int use_cmake_prefix)
{
    StringBuilder *sb = sb_create();
    WIN32_FIND_DATA find_data;
    HANDLE hFind;

    // Dynamic memory allocation
    size_t current_dir_len = strlen(current_dir);
    char *current_dir_win = malloc(current_dir_len + 2); // +2 for potential '\' and '\0'
    if (!current_dir_win)
    {
        sb_free(sb);
        return NULL;
    }

    // Add '\' at the end of current directory if not present
    if (current_dir[current_dir_len - 1] == '\\')
    {
        strcpy(current_dir_win, current_dir);
    }
    else
    {
        sprintf(current_dir_win, "%s\\", current_dir);
    }

    size_t original_base_dir_len = strlen(original_base_dir);
    char *original_base_dir_win = malloc(original_base_dir_len + 2);
    if (!original_base_dir_win)
    {
        free(current_dir_win);
        sb_free(sb);
        return NULL;
    }

    if (original_base_dir[original_base_dir_len - 1] == '\\')
    {
        strcpy(original_base_dir_win, original_base_dir);
    }
    else
    {
        sprintf(original_base_dir_win, "%s\\", original_base_dir);
    }

    // Allocate memory for search path
    size_t search_path_len = strlen(current_dir_win) + 2; // +1 for '*', +1 for '\0'
    char *search_path = malloc(search_path_len);
    if (!search_path)
    {
        free(current_dir_win);
        free(original_base_dir_win);
        sb_free(sb);
        return NULL;
    }
    sprintf(search_path, "%s*", current_dir_win);

    hFind = FindFirstFile(search_path, &find_data);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        free(current_dir_win);
        free(original_base_dir_win);
        free(search_path);
        sb_free(sb);
        return NULL;
    }

    do
    {
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0)
        {
            continue;
        }

        // Allocate memory for full path
        size_t filename_len = strlen(find_data.cFileName);
        size_t current_dir_win_len = strlen(current_dir_win);
        size_t full_path_len = current_dir_win_len + filename_len + 1;
        char *full_path = malloc(full_path_len);
        if (!full_path)
        {
            continue; // Skip this file, continue
        }
        sprintf(full_path, "%s%s", current_dir_win, find_data.cFileName);

        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            // Recursively search subdirectories
            char *sub_files = find_c_files_internal(full_path, original_base_dir, use_cmake_prefix);
            if (sub_files)
            {
                sb_append(sb, sub_files);
                free(sub_files);
            }
        }
        else
        {
            // Check for .c files
            const char *ext = strrchr(find_data.cFileName, '.');
            if (ext && strcmp(ext, ".c") == 0)
            {
                // Calculate relative path
                size_t original_base_len = strlen(original_base_dir_win);
                if (strlen(full_path) > original_base_len)
                {
                    const char *rel_start = full_path + original_base_len;

                    // Allocate memory for line
                    char *line = NULL;
                    size_t line_len = 0;

                    if (use_cmake_prefix)
                    {
                        line_len = strlen("    ${CMAKE_CURRENT_SOURCE_DIR}/") + strlen(rel_start) + 2;
                        line = malloc(line_len);
                        if (line)
                        {
                            sprintf(line, "    ${CMAKE_CURRENT_SOURCE_DIR}/%s\n", rel_start);
                        }
                    }
                    else if (!strcmp(original_base_dir, "core"))
                    {
                        line_len = 4 + strlen(rel_start) + 2;
                        line = malloc(line_len);
                        if (line)
                        {
                            sprintf(line, "    %s\n", rel_start);
                        }
                    }
                    else
                    {
                        line_len = 4 + filename_len + 2;
                        line = malloc(line_len);
                        if (line)
                        {
                            sprintf(line, "    %s\n", find_data.cFileName);
                        }
                    }

                    if (line)
                    {
                        // Convert Windows path separators to CMake format
                        char *p = line;
                        while ((p = strchr(p, '\\')) != NULL)
                        {
                            *p = '/';
                        }
                        sb_append(sb, line);
                        free(line);
                    }
                }
            }
        }
        free(full_path);
    } while (FindNextFile(hFind, &find_data));

    FindClose(hFind);
    free(current_dir_win);
    free(original_base_dir_win);
    free(search_path);

    char *result = malloc(strlen(sb->data) + 1);
    if (result)
    {
        strcpy(result, sb->data);
    }
    sb_free(sb);
    return result;
}

#else // Unix/Linux/macOS

char *find_c_files_internal(const char *current_dir, const char *original_base_dir, int use_cmake_prefix);

char *find_c_files(const char *base_dir, int use_cmake_prefix)
{
    return find_c_files_internal(base_dir, base_dir, use_cmake_prefix);
}

char *find_c_files_internal(const char *current_dir, const char *original_base_dir, int use_cmake_prefix)
{
    StringBuilder *sb = sb_create();
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;

    // Dynamic memory allocation
    size_t current_dir_len = strlen(current_dir);
    char *current_dir_unix = malloc(current_dir_len + 2); // +2 for potential '/' and '\0'
    if (!current_dir_unix)
    {
        sb_free(sb);
        return NULL;
    }

    // Add '/' at the end of current directory if not present
    if (current_dir[current_dir_len - 1] == '/')
    {
        strcpy(current_dir_unix, current_dir);
    }
    else
    {
        sprintf(current_dir_unix, "%s/", current_dir);
    }

    size_t original_base_dir_len = strlen(original_base_dir);
    char *original_base_dir_unix = malloc(original_base_dir_len + 2);
    if (!original_base_dir_unix)
    {
        free(current_dir_unix);
        sb_free(sb);
        return NULL;
    }

    // Add '/' to end of base directory if not present
    if (original_base_dir[original_base_dir_len - 1] == '/')
    {
        strcpy(original_base_dir_unix, original_base_dir);
    }
    else
    {
        sprintf(original_base_dir_unix, "%s/", original_base_dir);
    }

    dir = opendir(current_dir);
    if (!dir)
    {
        free(current_dir_unix);
        free(original_base_dir_unix);
        sb_free(sb);
        return NULL;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        // Allocate memory for full path
        size_t filename_len = strlen(entry->d_name);
        size_t current_dir_unix_len = strlen(current_dir_unix);
        size_t full_path_len = current_dir_unix_len + filename_len + 1;
        char *full_path = malloc(full_path_len);
        if (!full_path)
        {
            continue; // Skip this file, continue
        }
        sprintf(full_path, "%s%s", current_dir_unix, entry->d_name);

        if (stat(full_path, &file_stat) == 0)
        {
            if (S_ISDIR(file_stat.st_mode))
            {
                // Recursively search subdirectories
                char *sub_files = find_c_files_internal(full_path, original_base_dir, use_cmake_prefix);
                if (sub_files)
                {
                    sb_append(sb, sub_files);
                    free(sub_files);
                }
            }
            else if (S_ISREG(file_stat.st_mode))
            {
                // Check for .c extension
                const char *ext = strrchr(entry->d_name, '.');
                if (ext && strcmp(ext, ".c") == 0)
                {
                    // Calculate relative path
                    size_t original_base_len = strlen(original_base_dir_unix);
                    if (strlen(full_path) > original_base_len)
                    {
                        const char *rel_start = full_path + original_base_len;

                        char *line = NULL;
                        size_t line_len = 0;

                        if (use_cmake_prefix)
                        {
                            line_len = strlen("    ${CMAKE_CURRENT_SOURCE_DIR}/") + strlen(rel_start) + 2;
                            line = malloc(line_len);
                            if (line)
                            {
                                sprintf(line, "    ${CMAKE_CURRENT_SOURCE_DIR}/%s\n", rel_start);
                            }
                        }
                        else if (!strcmp(original_base_dir, "core"))
                        {
                            line_len = 4 + strlen(rel_start) + 2;
                            line = malloc(line_len);
                            if (line)
                            {
                                sprintf(line, "    %s\n", rel_start);
                            }
                        }
                        else
                        {
                            line_len = 4 + filename_len + 2;
                            line = malloc(line_len);
                            if (line)
                            {
                                sprintf(line, "    %s\n", entry->d_name);
                            }
                        }

                        if (line)
                        {
                            sb_append(sb, line);
                            free(line);
                        }
                    }
                }
            }
        }
        free(full_path);
    }

    closedir(dir);
    free(current_dir_unix);
    free(original_base_dir_unix);

    char *result = malloc(strlen(sb->data) + 1);
    if (result)
    {
        strcpy(result, sb->data);
    }
    sb_free(sb);
    return result;
}
#endif

char *generate_src_files_section(const char *dir)
{
    const char *base_dir = dir;
    char *c_files;

    // Use CMAKE prefix for src, but not for core directory
    int use_cmake_prefix = (strcmp(dir, "src") == 0) ? 1 : 0;

    // Find all .c files in the given directory, optionally prepending "${CMAKE_CURRENT_SOURCE_DIR}/"
    c_files = find_c_files(base_dir, use_cmake_prefix);

    if (!c_files)
    {
        return NULL;
    }

    // Wrap the list into a top-level CMake variable called SRC_FILES
    if (strcmp(dir, "core") == 0)
    {
        size_t total_len = strlen("set(SRC_FILES\n") + strlen(c_files) + strlen(")\n") + 1;
        char *result = malloc(total_len);

        snprintf(result, total_len, "set(SRC_FILES\n%s)", c_files);
        free(c_files);
        return result;
    }

    if (strcmp(dir, "src") == 0)
    {
        size_t total_len = strlen("set(APP_SRC\n") + strlen(c_files) + strlen("    PARENT_SCOPE\n)\n") + 1;
        char *result = malloc(total_len);

        snprintf(result, total_len, "set(APP_SRC\n%s    PARENT_SCOPE\n)", c_files);

        free(c_files);
        return result;
    }

    free(c_files);
    return NULL;
}

int update_cmake_file(const char *dir)
{
    char cmake_file[256];
    snprintf(cmake_file, sizeof(cmake_file), "%s%sCMakeLists.txt", dir, PATH_SEPARATOR);

    // Check if the src direction exists
    struct stat st;
    if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode))
    {
        printf("ERROR: Source directory '%s' not found!\n", dir);
        return -1;
    }

    // Read the contents of existing CMakeLists.txt
    char *content = read_file(cmake_file);
    if (!content)
    {
        printf("CMakeLists.txt couldn't be read: %s\n", cmake_file);
        return -1;
    }

    // Create a new SRC_FILES section
    char *new_src_files = generate_src_files_section(dir);
    if (!new_src_files)
    {
        printf("SRC_FILES list couldn't be created\n");
        free(content);
        return -1;
    }

    StringBuilder *new_content = sb_create();
    char *line_start = content;
    char *line_end;
    int in_src_files_section = 0;

    // Process line by line
    while ((line_end = strchr(line_start, '\n')) != NULL || *line_start != '\0')
    {
        char line[1024];

        if (line_end)
        {
            size_t line_len = line_end - line_start;
            // Windows \r\n handling
            if (line_len > 0 && line_start[line_len - 1] == '\r')
            {
                line_len--;
            }
            strncpy(line, line_start, line_len);
            line[line_len] = '\0';
            line_start = line_end + 1;
        }
        else
        {
            // The last line
            strcpy(line, line_start);
            line_start += strlen(line_start);
        }

        // Skip the existing SRC_FILES section
        if (contains_string(line, "set(SRC_FILES") || contains_string(line, "set(APP_SRC"))
        {
            in_src_files_section = 1;
            continue;
        }

        if (in_src_files_section)
        {
            if (strchr(line, ')'))
            {
                in_src_files_section = 0;
            }
            continue;
        }

        // Add SRC_FILES when the comment line is found
        if (contains_string(line, "# List of source files (do not touch this comment line)"))
        {
            sb_append(new_content, line);
            sb_append(new_content, "\n");
            sb_append(new_content, new_src_files);
            sb_append(new_content, "\n");
        }
        else
        {
            sb_append(new_content, line);
            sb_append(new_content, "\n");
        }

        if (*line_start == '\0')
            break;
    }

    // Write the updated content to file
    int result = write_file(cmake_file, new_content->data);

    // Cleanup
    free(content);
    free(new_src_files);
    sb_free(new_content);

    if (result == 0)
    {
        printf("CMakeLists.txt updated successfully\n");
    }
    else
    {
        printf("CMakeLists.txt couldn't be updated\n");
    }

    return result;
}

// Utility function to update "target_link_libraries"
char *add_to_target_link_libraries(const char *content, const char *lib_to_add)
{
    char *link_pos = strstr(content, "target_link_libraries(");
    if (!link_pos)
    {
        // If not found, create a copy so that the caller can free the buffer later:
        char *copy = malloc(strlen(content) + 1);
        if (!copy)
            return NULL;
        strcpy(copy, content);
        return copy;
    }

    char *paren_close = strchr(link_pos, ')');
    if (!paren_close)
    {
        // If ')' is not found, treat as error and return a copy:
        char *copy = malloc(strlen(content) + 1);
        if (!copy)
            return NULL;
        strcpy(copy, content);
        return copy;
    }

    // The parameter must be space-prefixed (e.g., " l8w8jwt")
    const char *to_insert = lib_to_add;

    // Calculate the required size for the new buffer:
    //    - Length of the original content
    //    - + Length of “to_insert”
    //    - + 1 for the terminating '\0'

    size_t orig_len = strlen(content);
    size_t insert_len = strlen(to_insert);
    size_t new_capacity = orig_len + insert_len + 1;
    char *temp = malloc(new_capacity);
    if (!temp)
    {
        return NULL;
    }

    // Copy everything up to (but not including) the closing parenthesis:
    size_t offset = (size_t)(paren_close - content);
    strncpy(temp, content, offset);
    temp[offset] = '\0';

    // Append the library name to insert
    strcat(temp, to_insert);

    // Append the closing parenthesis and the rest of the content:
    strcat(temp, paren_close);

    return temp;
}

// Handle TinyCBOR integration
void handle_cbor()
{
    const char *cmake_file = "core/CMakeLists.txt";

    printf("Checking TinyCBOR integration...\n");

    char *content = read_file(cmake_file);
    if (!content)
    {
        printf("Error: Cannot read CMakeLists.txt\n");
        return;
    }

    if (contains_string(content, "FetchContent_Declare") &&
        contains_string(content, "FetchContent_Declare(\n  tinycbor"))
    {
        printf("TinyCBOR is already added\n");
        free(content);
        return;
    }

    printf("Adding TinyCBOR...\n");

    const char *tinycbor_block =
        "FetchContent_Declare(\n"
        "  tinycbor\n"
        "  GIT_REPOSITORY https://github.com/intel/tinycbor.git\n"
        "  GIT_TAG main\n"
        ")\n"
        "FetchContent_MakeAvailable(tinycbor)\n";

    // Replace "# Empty place for TinyCBOR" with the TinyCBOR block
    char *new_content = malloc(strlen(content) + strlen(tinycbor_block) + 1000);
    if (!new_content)
    {
        free(content);
        return;
    }

    char *pos = strstr(content, "# Empty place for TinyCBOR (do not touch this comment line)");
    if (pos)
    {
        size_t prefix_len = pos - content;
        strncpy(new_content, content, prefix_len);
        new_content[prefix_len] = '\0';
        strcat(new_content, tinycbor_block);

        char *line_end = strchr(pos, '\n');
        if (line_end)
        {
            strcat(new_content, line_end + 1);
        }
    }
    else
    {
        strcpy(new_content, content);
    }

    char *updated = add_to_target_link_libraries(new_content, " tinycbor");
    if (!updated)
    {
        printf("Error: Adding TinyCBOR to target_link_libraries has failed\n");
        free(content);
        free(new_content);
        return;
    }

    free(new_content);
    new_content = updated;

    write_file(cmake_file, new_content);
    free(content);
    free(new_content);

    printf("TinyCBOR added successfully\n");
}

void handle_jwt()
{
    const char *cmake_file = "core/CMakeLists.txt";

    printf("Checking l8w8jwt integration...\n");

    char *content = read_file(cmake_file);
    if (!content)
    {
        printf("Error: Cannot read CMakeLists.txt\n");
        return;
    }

    if (contains_string(content, "add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/vendors/l8w8jwt)"))
    {
        printf("l8w8jwt is already added\n");
        free(content);
        return;
    }

    printf("Adding l8w8jwt...\n");

    // Install l8w8jwt as github submodule
    int ret;

    // Add it as github submodule
    ret = system("git submodule add https://github.com/GlitchedPolygons/l8w8jwt.git core/vendors/l8w8jwt");
    if (ret != 0)
    {
        fprintf(stderr, "Error: 'git submodule add' failed: %d\n", ret);
        return;
    }

    ret = system("git submodule update --init --recursive");
    if (ret != 0)
    {
        fprintf(stderr, "Error: 'git submodule update --init --recursive' failed: %d\n", ret);
        return;
    }

    // Add to CMake
    const char *l8w8jwt_block =
        "patch_cmake_minimum_required(\n"
        "  \"${CMAKE_CURRENT_SOURCE_DIR}/vendors/l8w8jwt/lib/mbedtls/CMakeLists.txt\"\n"
        "  \"mbedTLS\"\n"
        ")\n"
        "\n"
        "patch_cmake_minimum_required(\n"
        "   \"${CMAKE_CURRENT_SOURCE_DIR}/vendors/l8w8jwt/lib/chillbuff/CMakeLists.txt\"\n"
        "   \"chillbuff\"\n"
        ")\n"
        "\n"
        "add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/vendors/l8w8jwt)\n";

    // Replace "# Empty place for l8w8jwt" with the l8w8jwt block
    size_t new_capacity = strlen(content) + strlen(l8w8jwt_block) + 1;
    char *new_content = malloc(new_capacity);
    if (!new_content)
    {
        free(content);
        printf("Error: malloc failed when allocating new_content\n");
        return;
    }

    char *pos = strstr(content, "# Empty place for l8w8jwt (do not touch this comment line)");
    if (pos)
    {
        size_t prefix_len = (size_t)(pos - content);
        strncpy(new_content, content, prefix_len);
        new_content[prefix_len] = '\0';

        strcat(new_content, l8w8jwt_block);

        char *line_end = strchr(pos, '\n');
        if (line_end)
        {
            strcat(new_content, line_end + 1);
        }
    }
    else
    {
        strcpy(new_content, content);
    }

    // Replace target_link_libraries line
    char *updated = add_to_target_link_libraries(new_content, " l8w8jwt");
    if (!updated)
    {
        printf("Error: Adding l8w8jwt to target_link_libraries has failed\n");
        free(content);
        free(new_content);
        return;
    }

    free(new_content);
    new_content = updated;

    if (write_file(cmake_file, new_content) != 0)
    {
        printf("Error: Cannot write to %s\n", cmake_file);
    }
    else
    {
        printf("l8w8jwt added successfully\n");
    }

    free(content);
    free(new_content);

    printf("l8w8jwt added successfully\n");
}

// Install plugin function
int install_plugin(const char *plugin_name, const char *folder, const char *c_url, const char *h_url)
{
    char target_dir[256];
    snprintf(target_dir, sizeof(target_dir), "core%s%s", PATH_SEPARATOR, folder);

    printf("Installing %s\n", plugin_name);

    if (create_directory(target_dir) != 0)
    {
        printf("The target directory couldn't be created: %s\n", target_dir);
        return -1;
    }

    char c_path[256], h_path[256];
    build_plugin_path(c_path, sizeof(c_path), plugin_name, "c", folder);
    build_plugin_path(h_path, sizeof(h_path), plugin_name, "h", folder);

    if (download_file(c_url, c_path) != 0)
    {
        printf("Failed to download %s.c\n", plugin_name);
        return -1;
    }

    if (download_file(h_url, h_path) != 0)
    {
        printf("Failed to download %s.h\n", plugin_name);
        return -1;
    }

    printf("Installation completed to %s\n", target_dir);

    // Update CMake file
    return update_cmake_file("core");
}

//
//
// <---- COMMANDS ---->
//
//

// Create new project function
int create_project()
{
    // Get project name from user
    char project_name[256];
    printf("Create a project:\n");
    printf("Enter the project name >>> ");
    fflush(stdout);

    if (fgets(project_name, sizeof(project_name), stdin) == NULL)
    {
        printf("Error reading project name\n");
        return -1;
    }

    // Remove newline character
    size_t len = strlen(project_name);
    if (len > 0 && project_name[len - 1] == '\n')
    {
        project_name[len - 1] = '\0';
    }

    if (strlen(project_name) == 0)
    {
        printf("Project name cannot be empty\n");
        return -1;
    }

    // Select plugin
    interactive_select();

    printf("Creating project: %s\n", project_name);

    // Pull Tarball
    execute_command(
        "curl -L https://api.github.com/repos/savashn/ecewo/tarball/main "
        "-o tmp.tar.gz");

    // Copy "core", ".gitignore" and "CMakeLists.txt" to the root directory
    char tar_cmd[512];
    snprintf(tar_cmd, sizeof(tar_cmd),
             "tar -xzf tmp.tar.gz "
             "--strip-components=1 "
             "--wildcards "
             "'*/core/*' "
             "'*/CMakeLists.txt' "
             "'*/.gitignore' "
             "-C \"%s\"",
             project_name);
    execute_command(tar_cmd);

    // Delete temp archive
    execute_command("rm tmp.tar.gz");

    for (int i = 0; i < plugin_count; i++)
    {
        if (plugins[i].selected)
        {
            // Specific processes only for cbor and l8w8jwt
            if (strcmp(plugins[i].name, "cbor") == 0)
            {
                printf("Handling TinyCBOR integration...\n");
                handle_cbor();
                continue;
            }

            if (strcmp(plugins[i].name, "l8w8jwt") == 0)
            {
                printf("Handling l8w8jwt integration...\n");
                handle_jwt();
                continue;
            }

            // Installing process for the other plugins
            if (install_plugin(
                    plugins[i].name,
                    plugins[i].folder,
                    plugins[i].c_url,
                    plugins[i].h_url) != 0)
            {
                fprintf(stderr, "Error installing %s\n", plugins[i].name);
            }
        }
    }

    // Create src directory
    const char *src_dir = "src";
    create_directory(src_dir);

    // Create handlers.h
    const char *handlers_h_content =
        "#ifndef HANDLERS_H\n"
        "#define HANDLERS_H\n"
        "\n"
        "#include \"ecewo.h\"\n"
        "\n"
        "void hello_world(Req *req, Res *res);\n"
        "\n"
        "#endif\n";

    if (write_file("src/handlers.h", handlers_h_content) != 0)
    {
        printf("Error creating handlers.h\n");
        return -1;
    }

    // Create handlers.c
    const char *handlers_c_content =
        "#include \"handlers.h\"\n"
        "\n"
        "void hello_world(Req *req, Res *res)\n"
        "{\n"
        "  text(200, \"hello world!\");\n"
        "}\n";

    if (write_file("src/handlers.c", handlers_c_content) != 0)
    {
        printf("Error creating handlers.c\n");
        return -1;
    }

    // Create main.c
    const char *main_c_content =
        "#include \"server.h\"\n"
        "#include \"handlers.h\"\n"
        "\n"
        "int main()\n"
        "{\n"
        "  init_router();\n"
        "  get(\"/\", hello_world);\n"
        "  ecewo(4000);\n"
        "  final_router();\n"
        "  return 0;\n"
        "}\n";

    if (write_file("src/main.c", main_c_content) != 0)
    {
        printf("Error creating main.c\n");
        return -1;
    }

    // Create CMakeLists.txt with project name
    char cmake_content[1024];
    snprintf(cmake_content, sizeof(cmake_content),
             "cmake_minimum_required(VERSION 3.10)\n"
             "project(%s VERSION 0.1.0 LANGUAGES C)\n"
             "\n"
             "# List of source files (do not touch this comment line)\n"
             "set(APP_SRC\n"
             "  ${CMAKE_CURRENT_SOURCE_DIR}/main.c\n"
             "  ${CMAKE_CURRENT_SOURCE_DIR}/handlers.c\n"
             "  PARENT_SCOPE\n"
             ")\n",
             project_name);

    if (write_file("src/CMakeLists.txt", cmake_content) != 0)
    {
        printf("Error creating CMakeLists.txt\n");
        return -1;
    }

    printf("Starter project created successfully.\n");

    printf("Project '%s' created successfully!\n", project_name);
    printf("To build and run your project:\n");
#ifdef _WIN32
    printf("  ecewo --run\n");
    printf("  or\n");
    printf("  ./ecewo.exe --run\n");
#else
    printf("  ecewo --run\n");
    printf("  or\n");
    printf("  ./ecewo --run\n");
#endif

    return 0;
}

// Build and run function
int build_and_run()
{
    printf("Creating build directory...\n");
    create_directory("build");

    if (chdir("build") != 0)
    {
        printf("Error: Cannot change to build directory\n");
        return -1;
    }

    printf("Configuring with CMake...\n");
#ifdef _WIN32
    if (execute_command("cmake -G \"Visual Studio 17 2022\" ..") != 0)
    {
        if (execute_command("cmake -G \"MinGW Makefiles\" ..") != 0)
        {
            printf("Error: CMake configuration failed\n");
            chdir("..");
            return -1;
        }
    }
#else
    if (execute_command("cmake -G \"Unix Makefiles\" ..") != 0)
    {
        printf("Error: CMake configuration failed\n");
        chdir("..");
        return -1;
    }
#endif

    printf("Building...\n");
    if (execute_command("cmake --build . --config Release") != 0)
    {
        printf("Error: Build failed\n");
        chdir("..");
        return -1;
    }

    printf("Build completed!\n\n");
    printf("Running ecewo server...\n");

#ifdef _WIN32
    const char *server_exe = "Release\\server.exe";
    if (!file_exists(server_exe))
    {
        server_exe = "server.exe";
    }
#else
    const char *server_exe = "./server";
#endif

    if (file_exists(server_exe))
    {
        execute_command(server_exe);
    }
    else
    {
        printf("Server executable not found. Check for build errors.\n");
    }

    chdir("..");
    return 0;
}

int update_project()
{
    printf("Updating from %s (branch: main)\n", REPO_URL);

    // Check if CBOR library exists
    int cbor_enabled = 0;
    if (file_exists("core/CMakeLists.txt"))
    {
        char *content = read_file("core/CMakeLists.txt");
        if (content && contains_string(content, "FetchContent_MakeAvailable(tinycbor)"))
        {
            cbor_enabled = 1;
            printf("[Info] tinycbor integration found.\n");
        }
        free(content);
    }

    // Detect which plugins are installed
    int existing_count = 0;
    int *existing_plugins = malloc(plugin_count * sizeof(int));
    if (!existing_plugins)
    {
        printf("Memory allocation failed\n");
        return -1;
    }

    printf("Scanning for existing plugins...\n");

    for (int i = 0; i < plugin_count; i++)
    {
        // Skip for CBOR
        if (plugins[i].c_url == NULL || plugins[i].h_url == NULL)
        {
            existing_plugins[i] = 0;
            continue;
        }

        // Check the installed plugin files
        char c_path[256], h_path[256];
        build_plugin_path(c_path, sizeof(c_path), plugins[i].name, "c", plugins[i].folder);
        build_plugin_path(h_path, sizeof(h_path), plugins[i].name, "h", plugins[i].folder);

        // If there is at least one file (might be .c or .h) then existing_count++
        if (file_exists(c_path) || file_exists(h_path))
        {
            existing_plugins[i] = 1;
            existing_count++;
            printf("Found existing plugin: %s\n", plugins[i].name);
        }
        else
        {
            existing_plugins[i] = 0;
        }
    }

    printf("Found %d existing plugin(s) to reinstall.\n\n", existing_count);

    // Install the current tarball from github
    execute_command(
        "curl -L https://api.github.com/repos/savashn/ecewo/tarball/main "
        "-o tmp.tar.gz");

    // Extract into the current project directory (overwrites existing files)
    execute_command(
        "tar -xzf tmp.tar.gz "
        "--strip-components=1 "
        "--wildcards "
        "'*/core/*' "
        "'*/CMakeLists.txt' "
        "'*/.gitignore' "
        "'*/makefile' "
        "'*/cli.c' "
        "-C .");

    // Remove the temp archive
    execute_command("rm tmp.tar.gz");

    // Now reinstall the existing plugins
    printf("\nReinstalling existing plugins...\n");

    int success_count = 0;
    int current = 1;

    for (int i = 0; i < plugin_count; i++)
    {
        if (existing_plugins[i])
        {
            printf("\n[%d/%d] Reinstalling %s...\n", current, existing_count, plugins[i].name);

            if (install_plugin(plugins[i].name, plugins[i].folder,
                               plugins[i].c_url, plugins[i].h_url) == 0)
            {
                success_count++;
                printf("✓ %s reinstalled successfully\n", plugins[i].name);
            }
            else
            {
                printf("✗ Failed to reinstall %s\n", plugins[i].name);
            }
            current++;
        }
    }

    printf("\n=== Reinstallation Summary ===\n");
    printf("Successfully reinstalled: %d/%d plugins\n", success_count, existing_count);

    if (success_count == existing_count)
    {
        printf("All existing plugins reinstalled successfully!\n");
    }
    else
    {
        printf("%d plugin(s) failed to reinstall.\n", existing_count - success_count);
    }

    if (cbor_enabled)
    {
        printf("Running handle_cbor()...\n");
        handle_cbor();
    }

    free(existing_plugins);
    return 0;
}

// Show help

void show_install_help()
{
    printf("Plugins:\n");
    printf("=============================================\n");
    printf("  Async     ecewo install async\n");
    printf("  JSON      ecewo install cjson\n");
    printf("  CBOR      ecewo install cbor\n");
    printf("  dotenv    ecewo install dotenv\n");
    printf("  SQLite3   ecewo install sqlite\n");
    printf("  Cookie    ecewo install cookie\n");
    printf("  Session   ecewo install session\n");
    printf("  JWT       ecewo install l8w8jwt\n");
    printf("=============================================\n");
}

void show_help()
{
    printf("No parameters specified. Please use one of the following:\n");
    printf("==========================================================\n");
    printf("  ecewo create    # Create a new Ecewo project\n");
    printf("  ecewo run       # Build and run the project\n");
    printf("  ecewo rebuild   # Build from scratch\n");
    printf("  ecewo update    # Update Ecewo\n");
    printf("  ecewo migrate   # Migrate the CMakeLists.txt file\n");
    printf("  ecewo install   # Install packages\n");
    printf("==========================================================\n");

    show_install_help();
}

// Parse command line arguments
void parse_arguments(int argc, char *argv[], flags_t *flags)
{
    memset(flags, 0, sizeof(flags_t));

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "run") == 0)
        {
            flags->run = 1;
        }
        else if (strcmp(argv[i], "create") == 0)
        {
            flags->create = 1;
        }
        else if (strcmp(argv[i], "rebuild") == 0)
        {
            flags->rebuild = 1;
        }
        else if (strcmp(argv[i], "update") == 0)
        {
            flags->update = 1;
        }
        else if (strcmp(argv[i], "migrate") == 0)
        {
            flags->migrate = 1;
        }
        else if (strcmp(argv[i], "install") == 0)
        {
            flags->install = 1;
        }
        else if (strcmp(argv[i], "cjson") == 0)
        {
            flags->cjson = 1;
        }
        else if (strcmp(argv[i], "dotenv") == 0)
        {
            flags->dotenv = 1;
        }
        else if (strcmp(argv[i], "sqlite3") == 0)
        {
            flags->sqlite = 1;
        }
        else if (strcmp(argv[i], "cookie") == 0)
        {
            flags->cookie = 1;
        }
        else if (strcmp(argv[i], "session") == 0)
        {
            flags->session = 1;
        }
        else if (strcmp(argv[i], "async") == 0)
        {
            flags->async_plugin = 1;
        }
        else if (strcmp(argv[i], "cbor") == 0)
        {
            flags->cbor = 1;
        }
        else if (strcmp(argv[i], "l8w8jwt") == 0)
        {
            flags->l8w8jwt = 1;
        }
        else
        {
            printf("Unknown argument: %s\n", argv[i]);
        }
    }
}

int main(int argc, char *argv[])
{
    printf("Ecewo CLI for Linux, macOS and Windows\n");
    printf("2025 (c) Savas Sahin <savashn>\n\n");

    flags_t flags;
    parse_arguments(argc, argv, &flags);

    // Check if no parameters were provided
    if ((!flags.create && !flags.run && !flags.rebuild && !flags.update && !flags.migrate && !flags.install) || (flags.help))
    {
        show_help();
        return 0;
    }

    // Handle create command
    if (flags.create)
    {
        return create_project();
    }

    // Handle run command
    if (flags.run)
    {
        return build_and_run();
    }

    // Handle rebuild command
    if (flags.rebuild)
    {
        printf("Cleaning build directory...\n");
        remove_directory("build");
        printf("Rebuilding...\n\n");
        return build_and_run();
    }

    // Handle update command
    if (flags.update)
    {
        return update_project();
    }

    // Handle install command
    if (flags.install)
    {
        int has_plugin_arg = flags.cjson || flags.dotenv || flags.sqlite ||
                             flags.cookie || flags.session || flags.async_plugin || flags.cbor || flags.l8w8jwt;

        if (!has_plugin_arg)
        {
            show_install_help();
            return 0;
        }

        if (flags.cbor)
        {
            handle_cbor();
        }

        if (flags.l8w8jwt)
        {
            handle_jwt();
        }

        if (flags.cjson)
        {
            install_plugin("cJSON", "vendors", CJSON_C_URL, CJSON_H_URL);
        }

        if (flags.dotenv)
        {
            install_plugin("dotenv", "vendors", DOTENV_C_URL, DOTENV_H_URL);
        }

        if (flags.sqlite)
        {
            install_plugin("sqlite3", "vendors", SQLITE_C_URL, SQLITE_H_URL);
        }

        if (flags.cookie)
        {
            install_plugin("cookie", "plugins", COOKIE_C_URL, COOKIE_H_URL);
        }

        if (flags.session)
        {
            install_plugin("session", "plugins", SESSION_C_URL, SESSION_H_URL);
        }

        if (flags.async_plugin)
        {
            install_plugin("async", "plugins", ASYNC_C_URL, ASYNC_H_URL);
        }

        printf("Migration complete.\n");
        return 0;
    }

    if (flags.migrate)
    {
        return update_cmake_file("src");
    }

    return 0;
}
