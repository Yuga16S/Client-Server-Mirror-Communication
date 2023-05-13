#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/mman.h>
#include <limits.h>
#include <libgen.h>

#define PORT 8083
#define MAX_COMMAND_LENGTH 1024
#define MAX_RESPONSE_LENGTH 4096
#define MAX_FILE_NAME_LENGTH 256
#define MAX_BUFFER_SIZE 4096
#define MAX_PATH_LENGTH 1024


char * PROJ_HOME;

// #define PATH_MAX 1024

void processclient(int client_socket);
void send_error_msg(int client_sockfd, char *error_msg);

int send_FindFile_response(int sockfd, char *file_path);
int findFile(const char *rootPath, const char *fileName, char *outPath, size_t outPathSize);

void create_tarball_sGetFiles(char *output_filename, char *search_path, long size1, long size2);
void create_tarball_dGetFiles(char *output_filename, char *search_path, time_t date1, time_t date2);
void create_tarball_GetFiles(char *output_filename, char *search_path, char *filenames[]);
void create_tarball_Gettargz(char *output_filename, char *search_path, char *extensionList[]);
int send_file(int sockfd, const char *file_path);

int main(int argc, char const *argv[])
{
    PROJ_HOME = getenv("HOME");
    int server_socket, client_socket;
    struct sockaddr_in server_address, client_address;
    int addrlen = sizeof(server_address);

    // Create socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Set server address
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT);

    // Bind socket to address
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_socket, 100) < 0)
    {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    // Accept incoming connections and fork a child process to handle each one
    while (1)
    {
        if ((client_socket = accept(server_socket, (struct sockaddr *)&client_address, (socklen_t *)&addrlen)) < 0)
        {
            perror("accept failed");
            exit(EXIT_FAILURE);
        }

        pid_t pid = fork();
        if (pid < 0)
        {
            perror("fork failed");
            exit(EXIT_FAILURE);
        }
        else if (pid == 0)
        {
            // Child process
            close(server_socket);

            processclient(client_socket);

            exit(EXIT_SUCCESS);
        }
        else
        {
            // Parent process
            close(client_socket);
        }
    }

    return 0;
}

void send_error_msg(int client_sockfd, char *error_msg)
{
    // Send error message to client
    char response_msg[MAX_RESPONSE_LENGTH];
    snprintf(response_msg, MAX_RESPONSE_LENGTH, "ERROR: %s\n", error_msg);
    write(client_sockfd, response_msg, strlen(response_msg));
}

void processclient(int client_socket)
{
    char buffer[MAX_COMMAND_LENGTH];
    ssize_t bytes_read;

    while (1)
    {
        // Wait for a command from the client
        bytes_read = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes_read < 0)
        {
            perror("Error reading from socket");
            exit(1);
        }
        if (bytes_read == 0)
        {
            // Client disconnected
            close(client_socket);
            exit(0);
        }

        // Parse the command and execute it
        buffer[bytes_read] = '\0';
        printf("received command: %s\n", buffer);

        if (strncmp(buffer, "findfile ", 9) == 0)
        {
            char *filename = buffer + 9;
            send_FindFile_response(client_socket, filename);

            // printf("finish  send_FindFile_response %s\n", filename);
        }
        else if (strncmp(buffer, "sgetfiles ", 10) == 0)
        {
            char *token;
            long size1, size2;

            // Get the first token after "sgetfiles "
            token = strtok(buffer + 10, " ");
            size1 = strtol(token, NULL, 10);

            // Get the second token
            token = strtok(NULL, " ");
            size2 = strtol(token, NULL, 10);

            remove("temp.tar.gz");
            create_tarball_sGetFiles("temp.tar.gz", PROJ_HOME, size1, size2);
            send_file(client_socket, "temp.tar.gz");
        }
        else if (strncmp(buffer, "dgetfiles ", 10) == 0)
        {
            //printf("start processing dgetfiles: %s\n", buffer);

            char *token;
            char *date1_str = NULL, *date2_str = NULL;
            // Get the first token after "sgetfiles "
            token = strtok(buffer + 10, " ");
            date1_str = (char *)malloc(strlen(token) + 1);
            strcpy(date1_str, token);

            // Get the second token
            token = strtok(NULL, " ");
            date2_str = (char *)malloc(strlen(token) + 1);
            strcpy(date2_str, token);

            // printf("string dates, %s %s\n", date1_str, date2_str);

            struct tm date1_tm = {0}, date2_tm = {0};

            if (strptime(date1_str, "%Y-%m-%d", &date1_tm) == NULL)
            {
                printf("Invalid date format: %s\n", date1_str);
                continue;
            }

            if (strptime(date2_str, "%Y-%m-%d", &date2_tm) == NULL)
            {
                printf("Invalid date format: %s\n", date2_str);
                continue;
            }
            // printf("tm dates, %d-%d-%d %d-%d-%d\n\n",
            //        date1_tm.tm_year + 1900, date1_tm.tm_mon + 1, date1_tm.tm_mday,
            //        date2_tm.tm_year + 1900, date2_tm.tm_mon + 1, date2_tm.tm_mday);

            time_t date1_t = mktime(&date1_tm);
            time_t date2_t = mktime(&date2_tm);

            char date11_str[20], date12_str[20];
            strftime(date11_str, sizeof(date11_str), "%Y-%m-%d", localtime(&date1_t));
            strftime(date12_str, sizeof(date12_str), "%Y-%m-%d", localtime(&date2_t));

            // printf("time-t dates, %s %s\n\n", date11_str, date12_str);

            remove("temp.tar.gz");
            create_tarball_dGetFiles("temp.tar.gz", PROJ_HOME, date1_t, date2_t);
            send_file(client_socket, "temp.tar.gz");

            // Free memory allocated to date1_str and date2_str
            free(date1_str);
            free(date2_str);
        }
        else if (strncmp(buffer, "getfiles ", 9) == 0)
        {
            char *token;

            // Get the first token after "getfiles "
            char *filenames[6];
            filenames[0] = strtok(buffer + 9, " ");
            for (int i = 1; i < 6; i++)
            {
                filenames[i] = strtok(NULL, " ");
            }

            printf("getfiles received files: ");
            int num_files = sizeof(filenames) / sizeof(filenames[0]);
            for (int i = 0; i < num_files; i++)
            {
                printf("%s ", filenames[i]);
            }
            printf("\n");

            remove("temp.tar.gz");
            create_tarball_GetFiles("temp.tar.gz", PROJ_HOME, filenames);
            send_file(client_socket, "temp.tar.gz");
        }
        else if (strncmp(buffer, "gettargz ", 9) == 0)
        {
            char *token;

            // Get the first token after "getfiles "
            char *fileExtensions[6];
            fileExtensions[0] = strtok(buffer + 9, " ");
            for (int i = 1; i < 6; i++)
            {
                fileExtensions[i] = strtok(NULL, " ");
            }

            printf("gettargz received extensions: ");
            int num_extensions = sizeof(fileExtensions) / sizeof(fileExtensions[0]);
            for (int i = 0; i < num_extensions; i++)
            {
                printf("%s ", fileExtensions[i]);
            }
            printf("\n");

            remove("temp.tar.gz");
            create_tarball_Gettargz("temp.tar.gz", PROJ_HOME, fileExtensions);
            send_file(client_socket, "temp.tar.gz");
        }
        else if (strncmp(buffer, "quit", 4) == 0)
        {
            // Quit command received
            close(client_socket);
            exit(0);
        }
        else
        {
            // Invalid command
            send_error_msg(client_socket, "Invalid command");
        }
    }
}

int send_FindFile_response(int sockfd, char *fileName)
{
    // printf("enter send_FindFile_response %s\n", fileName);

    char filePath[PATH_MAX];
    if (findFile(PROJ_HOME, fileName, filePath, PATH_MAX) == -1)
    {
        printf("No file found %s\n", fileName);

        if (send(sockfd, "No file found", strlen("No file found"), 0) == -1)
        {
            perror("send");
            return -1;
        }
        return 0;
    }

    // printf("file found %s\n", filePath);
    struct stat filestat;
    if (stat(filePath, &filestat) == -1)
    {
        // perror("stat");
        return -1;
    }

    char size_str[20];
    snprintf(size_str, 20, "%ld", filestat.st_size);

    char date_str[30];
    strftime(date_str, 30, "%Y-%m-%d %H:%M:%S", localtime(&filestat.st_ctime));

    char response[PATH_MAX + 50];
    snprintf(response, PATH_MAX + 50, "%s,%s,%s", fileName, size_str, date_str);

    if (send(sockfd, response, strlen(response), 0) == -1)
    {
        perror("send");
        return -1;
    }

    return 0;
}

int findFile(const char *rootPath, const char *fileName, char *outPath, size_t outPathSize)
{
    // printf("enter findFile \n");

    DIR *dir;
    struct dirent *ent;
    char path[PATH_MAX];

    if ((dir = opendir(rootPath)) != NULL)
    {
        while ((ent = readdir(dir)) != NULL)
        {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            {
                continue;
            }

            snprintf(path, PATH_MAX, "%s/%s", rootPath, ent->d_name);

            struct stat st;
            if (stat(path, &st) == -1)
            {
                continue;
            }

            if (S_ISDIR(st.st_mode))
            {
                if (findFile(path, fileName, outPath, outPathSize) == 0)
                {
                    closedir(dir);
                    return 0;
                }
            }
            else if (S_ISREG(st.st_mode))
            {
                if (strcmp(ent->d_name, fileName) == 0)
                {
                    strncpy(outPath, path, outPathSize);
                    closedir(dir);
                    return 0;
                }
            }
        }

        // printf("Exit While loop \n");

        closedir(dir);
    }
    else
    {
        fprintf(stderr, "Cannot open directory %s: %s\n", rootPath, strerror(errno));
        return -1;
    }

    // printf("Exit findFile \n");

    return -1;
}

void create_tarball_sGetFiles(char *output_filename, char *search_path, long size1, long size2)
{
    
    // printf("enter create_tarball %s %s %ld %ld\n", output_filename, search_path, size1, size2);
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    char current_path[MAX_PATH_LENGTH];
    char tar_command[MAX_PATH_LENGTH + 50];
    long file_size;

    // Open the directory
    if ((dir = opendir(search_path)) == NULL)
    {
        perror("opendir");
        return;
    }

    // printf("create_tarball step1\n");

    // Loop through all entries in the directory
    while ((entry = readdir(dir)) != NULL)
    {
        // printf("create_tarball step2\n");

        // Build the full path to the entry
        snprintf(current_path, MAX_PATH_LENGTH, "%s/%s", search_path, entry->d_name);

        // Skip the current and parent directory entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            // printf("create_tarball Continue1 on directory\n");
            continue;
        }

        // Get the file's stat information
        if (stat(current_path, &statbuf) == -1)
        {
            // perror("stat");
            // printf("create_tarball Continue2 on error, current_Path: %s\n",current_path);
            continue;
        }

        // printf("create_tarball step3\n");

        // Check if the entry is a directory
        if (S_ISDIR(statbuf.st_mode))
        {
            // Recursively search the directory
            create_tarball_sGetFiles(output_filename, current_path, size1, size2);
        }
        // Check if the entry is a regular file
        else if (S_ISREG(statbuf.st_mode))
        {
            // printf("create_tarball step4\n");

            // Get the file size
            file_size = statbuf.st_size;

            // Check if the file size is within the specified range
            if (file_size >= size1 && file_size <= size2)
            {
                // printf("create_tarball step5\n");

                // Add the file to the tarball
                snprintf(tar_command, MAX_PATH_LENGTH + 50, "tar -rf '%s' '%s' > /dev/null 2>/dev/null", output_filename, current_path);
                system(tar_command);

                // printf("finish tar command: %s", tar_command);
            }
        }
    }

    // Close the directory
    closedir(dir);
}

void create_tarball_dGetFiles(char *output_filename, char *search_path, time_t date1, time_t date2)
{

    // char date11_str[20], date12_str[20];
    // strftime(date11_str, sizeof(date11_str), "%Y-%m-%d", localtime(&date1));
    // strftime(date12_str, sizeof(date12_str), "%Y-%m-%d", localtime(&date2));
    // printf("create_tarball  %s %s\n", date11_str, date12_str);

    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    char current_path[MAX_PATH_LENGTH];
    char tar_command[MAX_PATH_LENGTH + 50];
    long file_date;

    // Open the directory
    if ((dir = opendir(search_path)) == NULL)
    {
        perror("opendir");
        return;
    }

    // printf("create_tarball step1\n");

    // Loop through all entries in the directory
    while ((entry = readdir(dir)) != NULL)
    {
        // printf("create_tarball step2\n");

        // Build the full path to the entry
        snprintf(current_path, MAX_PATH_LENGTH, "%s/%s", search_path, entry->d_name);

        // Skip the current and parent directory entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            // printf("create_tarball Continue1 on directory\n");
            continue;
        }

        // Get the file's stat information
        if (stat(current_path, &statbuf) == -1)
        {
            // perror("stat");
            // printf("create_tarball Continue2 on error, current_Path: %s\n",current_path);
            continue;
        }

        // printf("create_tarball step3\n");

        // Check if the entry is a directory
        if (S_ISDIR(statbuf.st_mode))
        {
            // Recursively search the directory
            create_tarball_dGetFiles(output_filename, current_path, date1, date2);
        }
        // Check if the entry is a regular file
        else if (S_ISREG(statbuf.st_mode))
        {
            // printf("create_tarball step4\n");

            // Get the file creationTime
            file_date = statbuf.st_ctime;
            
            // Check if the file time is within the specified range
            long difDays1 = difftime(file_date, date1) / (60*60*24); // converting seconds to days
            long difDays2 = difftime(file_date, date2) / (60*60*24); // converting seconds to days
            if (difDays1 >= 0 && difDays2 <= 0) 
            {
                // printf("create_tarball step5\n");

                // Add the file to the tarball
                snprintf(tar_command, MAX_PATH_LENGTH + 50, "tar -rf '%s' '%s' > /dev/null 2>/dev/null", output_filename, current_path);
                system(tar_command);
                // printf("finish tar command \n");
            }
            // else
            // {
            //     char *file_date_str = ctime(&file_date);
            //     printf("old or new file with date: %s \n", file_date_str);
            // }
        }
    }

    // Close the directory
    closedir(dir);
}

void create_tarball_GetFiles(char *output_filename, char *search_path, char *filenames[])
{
    
    // printf("create_tarball  ");
    // for (int i = 0; i < 6; i++)
    // {
    //     printf("%s ", filenames[i]);
    // }
    // printf("\n");

    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    char current_path[MAX_PATH_LENGTH];
    char tar_command[MAX_PATH_LENGTH + 50];
    char *filename;

    // Open the directory
    if ((dir = opendir(search_path)) == NULL)
    {
        perror("opendir");
        return;
    }

    // printf("create_tarball step1\n");

    // Loop through all entries in the directory
    while ((entry = readdir(dir)) != NULL)
    {
        // printf("create_tarball step2\n");

        // Build the full path to the entry
        snprintf(current_path, MAX_PATH_LENGTH, "%s/%s", search_path, entry->d_name);

        // Skip the current and parent directory entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            // printf("create_tarball Continue1 on directory\n");
            continue;
        }

        // Get the file's stat information
        if (stat(current_path, &statbuf) == -1)
        {
            // perror("stat");
            // printf("create_tarball Continue2 on error, current_Path: %s\n",current_path);
            continue;
        }

        // printf("create_tarball step3\n");

        // Check if the entry is a directory
        if (S_ISDIR(statbuf.st_mode))
        {
            // Recursively search the directory
            create_tarball_GetFiles(output_filename, current_path, filenames);
        }
        // Check if the entry is a regular file
        else if (S_ISREG(statbuf.st_mode))
        {
            // printf("create_tarball step4\n");

            // Get the file name
            filename = basename(current_path);

            // Check if the filename is in the list of filenames
            int found = 0;
            for (int i = 0; i < 6; i++)
            {
                if (filenames[i] != NULL && strcmp(filenames[i], filename) == 0)
                {
                    found = 1;
                    break;
                }
            }

            if (found)
            {
                // printf("create_tarball step4\n");

                // Add the file to the tarball
                snprintf(tar_command, MAX_PATH_LENGTH + 50, "tar -rf '%s' '%s' > /dev/null 2>/dev/null", output_filename, current_path);
                system(tar_command);
                // printf("finish tar command \n");
            }
            else
            {
                // printf("file with different name: %s \n", filename);
            }
        }
    }

    // Close the directory
    closedir(dir);
}

void create_tarball_Gettargz(char *output_filename, char *search_path, char *extensionList[])
{
  

    // printf("create_tarball  ");
    // for (int i = 0; i < 6; i++)
    // {
    //     printf("%s ", extensionList[i]);
    // }
    // printf("\n");

    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    char current_path[MAX_PATH_LENGTH];
    char tar_command[MAX_PATH_LENGTH + 50];
    char *filename;
    char *fileExtension;
    // Open the directory
    if ((dir = opendir(search_path)) == NULL)
    {
        perror("opendir");
        return;
    }

    // printf("create_tarball step1\n");

    // Loop through all entries in the directory
    while ((entry = readdir(dir)) != NULL)
    {
        // printf("create_tarball step2\n");

        // Build the full path to the entry
        snprintf(current_path, MAX_PATH_LENGTH, "%s/%s", search_path, entry->d_name);

        // Skip the current and parent directory entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            // printf("create_tarball Continue1 on directory\n");
            continue;
        }

        // Get the file's stat information
        if (stat(current_path, &statbuf) == -1)
        {
            // perror("stat");
            // printf("create_tarball Continue2 on error, current_Path: %s\n",current_path);
            continue;
        }

        // printf("create_tarball step3\n");

        // Check if the entry is a directory
        if (S_ISDIR(statbuf.st_mode))
        {
            // Recursively search the directory
            create_tarball_Gettargz(output_filename, current_path, extensionList);
        }
        // Check if the entry is a regular file
        else if (S_ISREG(statbuf.st_mode))
        {
            // printf("create_tarball step4\n");

            // Get the file name
            filename = basename(current_path);
            fileExtension = strrchr(filename, '.');

            // printf("check file: %s  with extension: %s \n", filename, fileExtension); // +1 to remove . from extension

            // Check if the filename is in the list of filenames
            int found = 0;
            for (int i = 0; i < 6; i++)
            {
                if (extensionList[i] != NULL && fileExtension != NULL && strcmp(extensionList[i], fileExtension + 1) == 0)
                {
                    found = 1;
                    break;
                }
            }

            if (found)
            {
                // printf("create_tarball step4\n");

                // Add the file to the tarball
                snprintf(tar_command, MAX_PATH_LENGTH + 50, "tar -rf '%s' '%s' > /dev/null 2>/dev/null", output_filename, current_path);
                system(tar_command);
                // printf("finish tar command \n");
            }
            else
            {
                // printf("file with different extension: %s \n", fileExtension);
            }
        }
    }

    // Close the directory
    closedir(dir);
}

int send_file(int sockfd, const char *file_path)
{

    // Open the file
    FILE *file = fopen(file_path, "rb");
    if (!file)
    {
        // perror("fopen");
        // return -1;
        printf("No file found\n");

        int zero_file_size=0;
        if (send(sockfd, &zero_file_size, sizeof(zero_file_size), 0) == -1)
        {
            perror("send");
            return -1;
        }
        return 0;
    }

    // Get the file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Convert to network byte order
    long net_file_size = htonl(file_size);

    //printf("start send file: %s with size: %ld\n", file_path, net_file_size);

    // Send the file size to the client
    if (send(sockfd, &net_file_size, sizeof(net_file_size), 0) == -1)
    {
        perror("send");
        fclose(file);
        return -1;
    }
    // printf("send filesize in byte: %ld\n", file_size);

    // Send the file contents to the client
    char buffer[102400];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0)
    {
        // printf("file bytes: %ld \n", bytes_read);
        if (send(sockfd, buffer, bytes_read, 0) == -1)
        {
            perror("send");
            fclose(file);
            return -1;
        }
    }

    // Close the file
    fclose(file);

    // Send a message to indicate the file transfer is complete
    const char *message = "File transfer complete";
    if (send(sockfd, message, strlen(message), 0) == -1)
    {
        perror("send");
        return -1;
    }
    return 0;
}
