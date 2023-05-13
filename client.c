#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdbool.h>
#include <ctype.h>

#define BUFFER_SIZE 1024

#define SERVER_PORT 8082
#define MIRROR_PORT 8083

const char *SERVER_ADDRESS = "127.0.0.1";
const char *MIRROR_ADDRESS = "127.0.0.1";

const char *SERVER_TOKEN = "$$server$$";
const char *MIRROR_TOKEN = "$$mirror$$";

int main()
{
    int client_socket, err;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    char buffer_back[BUFFER_SIZE];

    // Create socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0)
    {
        perror("Failed to create socket");
        exit(1);
    }

    // Set server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
    server_addr.sin_port = htons(SERVER_PORT);

    // Connect to server
    err = connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (err < 0)
    {
        perror("Failed to connect to server");
        exit(1);
    }

    memset(buffer, 0, BUFFER_SIZE);
    err = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (err < 0)
    {
        perror("Failed to connect to server");
        exit(1);
    }
    else if (strcmp(buffer, SERVER_TOKEN) == 0)
    {
        // connected to server
    }
    else if (strcmp(buffer, MIRROR_TOKEN) == 0)
    {
        // server redirects request to mirror
        close(client_socket);

        // Create socket for mirror
        client_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (client_socket < 0)
        {
            perror("Failed to create socket for mirror");
            exit(1);
        }

        // Connect to mirror
        server_addr.sin_addr.s_addr = inet_addr(MIRROR_ADDRESS);
        server_addr.sin_port = htons(MIRROR_PORT);

        err = connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (err < 0)
        {
            perror("Failed to connect to mirror");
            exit(1);
        }
    }
    else
    {
        perror("Failed to connect to server or mirror");
        exit(1);
    }

    // Wait for user input
    while (1)
    {
        printf("C$ ");
        gets(buffer);
        memcpy(buffer_back, buffer, sizeof buffer);

        strtok(buffer, "\n"); // Remove newline character
        // printf("buffer received %s\n", buffer);

        //  Parse command and arguments
        char *command = strtok(buffer, " ");

        // printf("input command is %s\n", command);

        //  Verify command syntax
        if (strcmp(command, "findfile") == 0)
        {
            char *arg1 = strtok(NULL, " ");
            if (arg1 == NULL)
            {
                // Print error message
                printf("Invalid command syntax. Usage: findfile filename\n");
            }

            // printf("buffer to send %s\n", buffer_back);

            // Send command to server
            err = send(client_socket, buffer_back, strlen(buffer_back), 0);
            if (err < 0)
            {
                perror("Failed to send command to server");
                // exit(1);
            }

            // Receive response from server
            memset(buffer, 0, BUFFER_SIZE);
            err = recv(client_socket, buffer, BUFFER_SIZE, 0);
            if (err < 0)
            {
                perror("Failed to receive response from server");
                continue;
            }

            // Print response
            printf("%s\n", buffer);
        }
        else if (strcmp(command, "sgetfiles") == 0)
        {
            printf("command %s\n", buffer_back);

            char *size1 = strtok(NULL, " ");
            char *size2 = strtok(NULL, " ");
            char *uOption = strtok(NULL, " ");

            bool unzip = uOption != NULL && strcmp(uOption, "-u") == 0;

            int validinput = 1;

            int i = 0;
            while (size1[i] != '\0')
            {
                if (!isdigit(size1[i]))
                    validinput = 0;
                i++;
            }

            if (validinput == 0)
            {
                printf("Invalid input: '%s' is not an integer\n", size1);
                continue;
            }

            int j = 0;
            while (size1[j] != '\0')
            {
                if (!isdigit(size2[j]))
                    validinput = 0;
                j++;
            }

            if (validinput == 0)
            {
                printf("Invalid input: '%s' is not an integer\n", size2);
                continue;
            }
            
            int size1_int = atoi(size1);
            int size2_int = atoi(size2);

            // printf("sgetfiles %s arg1:%s arg2:%s \n", command, arg1, arg2);

            if (size1_int == NULL || size2_int == NULL)
            {
                // Print error message
                printf("Invalid command syntax. Usage: sgetfiles Size1 Size2\n");
                continue;
            }
            if (size1_int < 0 || size2_int < 0 || size1_int > size2_int)
            {
                printf("Invalid size range: %s-%s (size1 < = size2 (size1>= 0 and size2>=0))\n", size1, size2);
                continue;
            }

            // printf("buffer to send %s\n", buffer_back);

            // Send command to server
            err = send(client_socket, buffer_back, strlen(buffer_back), 0);
            if (err < 0)
            {
                perror("Failed to send command to server");
                continue;
            }

            // Receive response from server
            memset(buffer, 0, BUFFER_SIZE);

            // Receive file size
            long net_file_size;
            if (recv(client_socket, &net_file_size, sizeof(net_file_size), 0) == -1)
            {
                perror("recv");
                continue;
            }

            // printf("first response \n");

            // Convert to host byte order
            long file_size = ntohl(net_file_size);
            // printf("receive filesize in byte: %ld\n", file_size);

            // Receive file contents
            FILE *file = fopen("temp.tar.gz", "w");
            if (!file)
            {
                perror("fopen");
                return -1;
            }

            char buffer[file_size];
            long total_bytes_read = 0;
            while (total_bytes_read < file_size)
            {
                size_t bytes_to_read = sizeof(buffer);
                if (total_bytes_read + bytes_to_read > file_size)
                {
                    bytes_to_read = file_size - total_bytes_read;
                }
                ssize_t bytes_read = recv(client_socket, buffer, bytes_to_read, 0);
                if (bytes_read == -1)
                {
                    perror("recv");
                    fclose(file);
                    continue;
                }
                else if (bytes_read == 0)
                {
                    printf("Connection closed by server\n");
                    fclose(file);
                    continue;
                }
                total_bytes_read += bytes_read;

                if (fwrite(buffer, 1, bytes_read, file) != bytes_read)
                {
                    perror("fwrite");
                    fclose(file);
                    continue;
                }
            }

            // Receive a message indicating the file transfer is complete
            const int MESSAGE_SIZE = 256;
            char message[MESSAGE_SIZE];
            if (recv(client_socket, message, MESSAGE_SIZE, 0) == -1)
            {
                perror("recv");
                fclose(file);
                return -1;
            }
            printf("%s\n", message);

            fclose(file);

            if (unzip)
            {
                system("tar -xf temp.tar.gz > /dev/null && rm temp.tar.gz");
            }
        }
        else if (strcmp(command, "dgetfiles") == 0)
        {
            char *date1_str = strtok(NULL, " ");
            char *date2_str = strtok(NULL, " ");
            char *uOption = strtok(NULL, " ");

            bool unzip = uOption != NULL && strcmp(uOption, "-u") == 0;

            if (date1_str == NULL || date2_str == NULL)
            {
                printf("Invalid command syntax. Usage: dgetfiles date1 date2\n");
                continue;
            }



            // printf("string dates, %s %s\n\n", date1_str, date2_str);

            struct tm date1_tm = {0}, date2_tm = {0};

            if (strptime(date1_str, "%Y-%m-%d", &date1_tm) == NULL)
            {
                printf("Invalid date format: %s\n", date1_str);
                return 1;
            }

            if (strptime(date2_str, "%Y-%m-%d", &date2_tm) == NULL)
            {
                printf("Invalid date format: %s\n", date2_str);
                return 1;
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

            if (date1_t == -1)
            {
                printf("Invalid date1: %s\n", date1_str);
                return 1;
            }

            if (date2_t == -1)
            {
                printf("Invalid date2: %s\n", date2_str);
                return 1;
            }

            if (date1_t > date2_t)
            {
                printf("Invalid date range: %ld %ld\n", date1_t, date2_t);
                continue;
            }

            // printf("start operation, buffer to send, %s\n", buffer_back);

            // Send command to server
            err = send(client_socket, buffer_back, strlen(buffer_back), 0);
            if (err < 0)
            {
                perror("Failed to send command to server");
                // exit(1);
                continue;
            }

            // Receive response from server
            memset(buffer, 0, BUFFER_SIZE);

            // Receive file size
            long net_file_size;
            if (recv(client_socket, &net_file_size, sizeof(net_file_size), 0) == -1)
            {
                perror("recv");
                return -1;
            }

            // Convert to host byte order
            long file_size = ntohl(net_file_size);

            if (file_size <= 0)
            {
                printf("No file found\n");
                continue;
            }

            //printf("receive filesize in byte: %ld\n", file_size);

            // Receive file contents
            FILE *file = fopen("temp.tar.gz", "w");
            if (!file)
            {
                perror("fopen");
                return -1;
            }

            char buffer[file_size];
            long total_bytes_read = 0;
            while (total_bytes_read < file_size)
            {
                size_t bytes_to_read = sizeof(buffer);
                if (total_bytes_read + bytes_to_read > file_size)
                {
                    bytes_to_read = file_size - total_bytes_read;
                }
                ssize_t bytes_read = recv(client_socket, buffer, bytes_to_read, 0);
                if (bytes_read == -1)
                {
                    perror("recv");
                    fclose(file);
                    return -1;
                }
                else if (bytes_read == 0)
                {
                    printf("Connection closed by server\n");
                    fclose(file);
                    return -1;
                }
                total_bytes_read += bytes_read;

                if (fwrite(buffer, 1, bytes_read, file) != bytes_read)
                {
                    perror("fwrite");
                    fclose(file);
                    return -1;
                }
            }

            // Receive a message indicating the file transfer is complete
            const int MESSAGE_SIZE = 256;
            char message[MESSAGE_SIZE];
            if (recv(client_socket, message, MESSAGE_SIZE, 0) == -1)
            {
                perror("recv");
                fclose(file);
                return -1;
            }
            printf("%s\n", message);

            fclose(file);

            if (unzip)
            {
                system("tar -xf temp.tar.gz > /dev/null && rm temp.tar.gz");
            }
        }
        else if (strcmp(command, "getfiles") == 0)
        {
            char *file1_str = strtok(NULL, " ");
            char *file2_str = strtok(NULL, " ");
            char *file3_str = strtok(NULL, " ");
            char *file4_str = strtok(NULL, " ");
            char *file5_str = strtok(NULL, " ");
            char *file6_str = strtok(NULL, " ");
            char *arg7_str = strtok(NULL, " ");

            bool unzip = false;

            if (file1_str != NULL && strcmp(file1_str, "-u") == 0)
            {
                file1_str = NULL;
                unzip = true;
            }
            else if (file2_str != NULL && strcmp(file2_str, "-u") == 0)
            {
                file2_str = NULL;
                unzip = true;
            }
            else if (file3_str != NULL && strcmp(file3_str, "-u") == 0)
            {
                file3_str = NULL;
                unzip = true;
            }
            else if (file4_str != NULL && strcmp(file4_str, "-u") == 0)
            {
                file4_str = NULL;
                unzip = true;
            }
            else if (file5_str != NULL && strcmp(file5_str, "-u") == 0)
            {
                file5_str = NULL;
                unzip = true;
            }
            else if (file6_str != NULL && strcmp(file6_str, "-u") == 0)
            {
                file6_str = NULL;
                unzip = true;
            }
            else if (arg7_str!= NULL && strcmp(arg7_str, "-u") == 0)
            {
                unzip = true;
            }
            else if ((arg7_str!= NULL && strcmp(arg7_str, "-u") != 0))
            {
                printf("Invalid command syntax\n");
                continue;
            }

            if (file1_str == NULL && file2_str == NULL && file3_str == NULL && file4_str == NULL && file5_str == NULL && file6_str == NULL)
            {
                printf("Invalid command syntax. Usage: getfiles file1 file2 file3 file4 file5 file6 -u\n");
                continue;
            }

            printf("string files, %s %s %s %s %s %s %s\n\n", file1_str, file2_str, file3_str, file4_str, file5_str, file6_str, arg7_str);

            // printf("start getfiles operation, buffer to send, %s\n", buffer_back);

            // Send command to server
            err = send(client_socket, buffer_back, strlen(buffer_back), 0);
            if (err < 0)
            {
                perror("Failed to send command to server");
                // exit(1);
            }

            // Receive response from server
            memset(buffer, 0, BUFFER_SIZE);

            // Receive file size
            long net_file_size;
            if (recv(client_socket, &net_file_size, sizeof(net_file_size), 0) == -1)
            {
                perror("recv");
                return -1;
            }

            // Convert to host byte order
            long file_size = ntohl(net_file_size);

            if (file_size <= 0)
            {
                printf("No file found\n");
                continue;
            }

            //printf("receive filesize in byte: %ld\n", file_size);

            // Receive file contents
            FILE *file = fopen("temp.tar.gz", "w");
            if (!file)
            {
                perror("fopen");
                return -1;
            }

            char buffer[file_size];
            long total_bytes_read = 0;
            while (total_bytes_read < file_size)
            {
                size_t bytes_to_read = sizeof(buffer);
                if (total_bytes_read + bytes_to_read > file_size)
                {
                    bytes_to_read = file_size - total_bytes_read;
                }
                ssize_t bytes_read = recv(client_socket, buffer, bytes_to_read, 0);
                if (bytes_read == -1)
                {
                    perror("recv");
                    fclose(file);
                    return -1;
                }
                else if (bytes_read == 0)
                {
                    printf("Connection closed by server\n");
                    fclose(file);
                    return -1;
                }
                total_bytes_read += bytes_read;

                if (fwrite(buffer, 1, bytes_read, file) != bytes_read)
                {
                    perror("fwrite");
                    fclose(file);
                    return -1;
                }
            }

            // Receive a message indicating the file transfer is complete
            const int MESSAGE_SIZE = 256;
            char message[MESSAGE_SIZE];
            if (recv(client_socket, message, MESSAGE_SIZE, 0) == -1)
            {
                perror("recv");
                fclose(file);
                return -1;
            }
            printf("%s\n", message);

            fclose(file);

            if (unzip)
            {
                system("tar -xf temp.tar.gz > /dev/null && rm temp.tar.gz");
            }
        }
        else if (strcmp(command, "gettargz") == 0)
        {
            char *ext1_str = strtok(NULL, " ");
            char *ext2_str = strtok(NULL, " ");
            char *ext3_str = strtok(NULL, " ");
            char *ext4_str = strtok(NULL, " ");
            char *ext5_str = strtok(NULL, " ");
            char *ext6_str = strtok(NULL, " ");
            char *arg7_str = strtok(NULL, " ");

            bool unzip = false;

            if (ext1_str != NULL && strcmp(ext1_str, "-u") == 0)
            {
                ext1_str = NULL;
                unzip = true;
            }
            else if (ext2_str != NULL && strcmp(ext2_str, "-u") == 0)
            {
                ext2_str = NULL;
                unzip = true;
            }
            else if (ext3_str != NULL && strcmp(ext3_str, "-u") == 0)
            {
                ext3_str = NULL;
                unzip = true;
            }
            else if (ext4_str != NULL && strcmp(ext4_str, "-u") == 0)
            {
                ext4_str = NULL;
                unzip = true;
            }
            else if (ext5_str != NULL && strcmp(ext5_str, "-u") == 0)
            {
                ext5_str = NULL;
                unzip = true;
            }
            else if (ext6_str != NULL && strcmp(ext6_str, "-u") == 0)
            {
                ext6_str = NULL;
                unzip = true;
            }
            else if (arg7_str!= NULL && strcmp(arg7_str, "-u") == 0)
            {
                unzip = true;
            }
            else if ((arg7_str!= NULL && strcmp(arg7_str, "-u") != 0))
            {
                printf("Invalid command syntax\n");
                continue;
            }

            if (ext1_str == NULL && ext2_str == NULL && ext3_str == NULL && ext4_str == NULL && ext5_str == NULL && ext6_str == NULL)
            {
                printf("Invalid command syntax. Usage: gettargz <extension list> <-u>\n");
                continue;
            }

            printf("gettargz, string extensions, %s %s %s %s %s %s %s\n\n",
                   ext1_str, ext2_str, ext3_str, ext4_str, ext5_str, ext6_str, arg7_str);

            //printf("start gettargz operation, buffer to send, %s\n", buffer_back);

            // Send command to server
            err = send(client_socket, buffer_back, strlen(buffer_back), 0);
            if (err < 0)
            {
                perror("Failed to send command to server");
                // exit(1);
            }

            // Receive response from server
            memset(buffer, 0, BUFFER_SIZE);

            // Receive file size
            long net_file_size;
            if (recv(client_socket, &net_file_size, sizeof(net_file_size), 0) == -1)
            {
                perror("recv");
                return -1;
            }

            // Convert to host byte order
            long file_size = ntohl(net_file_size);

            if (file_size <= 0)
            {
                printf("No file found\n");
                continue;
            }
            printf("receive filesize in byte: %ld\n", file_size);

            // Receive file contents
            FILE *file = fopen("temp.tar.gz", "w");
            if (!file)
            {
                perror("fopen");
                return -1;
            }

            char buffer[file_size];
            long total_bytes_read = 0;
            while (total_bytes_read < file_size)
            {
                size_t bytes_to_read = sizeof(buffer);
                if (total_bytes_read + bytes_to_read > file_size)
                {
                    bytes_to_read = file_size - total_bytes_read;
                }
                ssize_t bytes_read = recv(client_socket, buffer, bytes_to_read, 0);
                if (bytes_read == -1)
                {
                    perror("recv");
                    fclose(file);
                    return -1;
                }
                else if (bytes_read == 0)
                {
                    printf("Connection closed by server\n");
                    fclose(file);
                    return -1;
                }
                total_bytes_read += bytes_read;

                if (fwrite(buffer, 1, bytes_read, file) != bytes_read)
                {
                    perror("fwrite");
                    fclose(file);
                    return -1;
                }
            }

            // Receive a message indicating the file transfer is complete
            const int MESSAGE_SIZE = 256;
            char message[MESSAGE_SIZE];
            if (recv(client_socket, message, MESSAGE_SIZE, 0) == -1)
            {
                perror("recv");
                fclose(file);
                return -1;
            }
            printf("%s\n", message);

            fclose(file);

            if (unzip)
            {
                system("tar -xf temp.tar.gz > /dev/null && rm temp.tar.gz");
            }
        }
        else if (strcmp(command, "quit") == 0)
        {
            printf("Exit Client App\n");
            return 0;
        }
        else
        {
            // Print error message
            printf("\n Invalid command syntax. please enter one of the following commands \n %s %s %s %s %s %s",
                   "1) findfile filename\n",
                   "2) sgetfiles size1 size2 <-u>\n",
                   "3) dgetfiles date1 date2 <-u>\n",
                   "4) getfiles file1 <file2 file3 file4 file5 file6> <-u >\n",
                   "5) gettargz <extension list> <-u>\n",
                   "6) quit\n");
        }
    }

    // Close socket
    close(client_socket);

    return 0;
}
