/*
 * This programs generates bin files from a live memory dump of any
 * microcorruption ctf lvl
 * Copyright (C) github.com/araml
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <fcntl.h>
#include <unistd.h>

char ascii_table [] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
};

#define END_OF_FILE -1

char hex_to_dec(char value) {
    if (value < 65)
        return ascii_table[value - 48];
    else
        return ascii_table[value - 87];
}

int convert_to_int(char *buf) {
    int result = 0;
    result = hex_to_dec(buf[0]) * 4096 +
             hex_to_dec(buf[1]) * 256  +
             hex_to_dec(buf[2]) * 16   +
             hex_to_dec(buf[3]);

    return result;
}

int get_initial_address(int fd) {
    char addr[4];
    off_t curr_offset = lseek(fd, 0, SEEK_CUR);
    if (!pread(fd, addr, 4, curr_offset))
        return END_OF_FILE;

    return convert_to_int(addr);
}

const int line_size = 16;
const char zeroes[] = {0, 0, 0, 0, 0, 0, 0, 0,
                       0, 0, 0, 0, 0, 0, 0, 0};

void zero(char *buf, size_t size) {
    for (size_t i = 0; i < size; i++) {
        buf[i] = 0;
    }
}

// TODO: don't drop line if it is not empty
int drop_line(int input_fd) {
    char c;
    while (1) {
        if(!read(input_fd, &c, 1))
            return END_OF_FILE;

        if (c == '\n')
            return 0;
    }
}

// Asume address hasn't been consumed by read.
int read_line(int fd, char *buf) {
    lseek(fd, 8, SEEK_CUR); // consume address and empty space.
    // There are 16 bytes in 2 bytes sections tokens per line
    unsigned char c;
    unsigned char tmp[32];
    for (int k = 0; k < 8; k++) {
        off_t curr_offset = lseek(fd, 0, SEEK_CUR);
        if (!pread(fd, &c, 1, curr_offset))
            return END_OF_FILE;

        if (c == '*') {
            buf[0] = '*';
            drop_line(fd);
            return drop_line(fd);
        }

        read(fd, &tmp[k * 4], 4);
        lseek(fd, 1, SEEK_CUR);
    }

    while (c != '\n') {
        read(fd, &c, 1);
    }

    for (int i = 0; i < 16; i++) {
        char value = hex_to_dec(tmp[2 * i]) * 16 +
                     hex_to_dec(tmp[(2 * i) + 1]);
        buf[i] = value;
    }

    return drop_line(fd);
}

void fill_with_zeroes(int output_fd, int current_address, int next_address) {
    for (; current_address < next_address; current_address +=16)
        write(output_fd, zeroes, sizeof(zeroes));
}

int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 2) {
        char error_buff[] = "Must give an input binary file";
        write(1, error_buff, sizeof(error_buff));
    }

    int input_fd = open(argv[1], O_RDONLY);

    int output_fd = open("output.bin", O_CREAT | O_TRUNC | O_WRONLY,
                         S_IRUSR | S_IWUSR | S_IRGRP |
                         S_IWGRP | S_IROTH | S_IWOTH);

    if (input_fd == -1 || output_fd == -1) {
        _exit(-1);
    }

    char line[80];

    while (1) {
        zero(line, 80);
        int address = 0;

        address = get_initial_address(input_fd);
        if (read_line(input_fd, line) == END_OF_FILE)
            break;

        if (line[0] == '*') {
            int next_address = get_initial_address(input_fd);
            fill_with_zeroes(output_fd, address, next_address);
        } else {
            write(output_fd, line, line_size);
        }
    }
}
