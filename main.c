#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <immintrin.h>

typedef char i8;
typedef unsigned char u8;
typedef unsigned short u16;
typedef int i32;
typedef unsigned u32;
typedef unsigned long u64;

#define PRINT_ERROR(cstring) write(STDERR_FILENO, cstring, sizeof(cstring) - 1)

#pragma pack(1)
struct bmp_header
{
	// Note: header
	i8  signature[2]; // should equal to "BM"
	u32 file_size;
	u32 unused_0;
	u32 data_offset;

	// Note: info header
	u32 info_header_size;
	u32 width; // in px
	u32 height; // in px
	u16 number_of_planes; // should be 1
	u16 bit_per_pixel; // 1, 4, 8, 16, 24 or 32
	u32 compression_type; // should be 0
	u32 compressed_image_size; // should be 0
	// Note: there are more stuff there but it is not important here
};

struct file_content
{
	i8*   data;
	u32   size;
};

struct file_content   read_entire_file(char* filename)
{
	char* file_data = 0;
	unsigned long	file_size = 0;
	int input_file_fd = open(filename, O_RDONLY);
	if (input_file_fd >= 0)
	{
		struct stat input_file_stat = {0};
		stat(filename, &input_file_stat);
		file_size = input_file_stat.st_size;
		file_data = mmap(0, file_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, input_file_fd, 0);
		close(input_file_fd);
	}
	return (struct file_content){file_data, file_size};
}

void print_bgr(u32 value)
{
	int blue = value & 0xFF;
	int green = (value >> 8) & 0xFF;
	int red = (value >> 16) & 0xFF;

	// printf("BGR=(%d, %d, %d)\n", blue, green, red);
	printf("RGB=(%d, %d, %d)\n", red, green, blue);
}

int	is_header(u32 value)
{
	int blue = value & 0xFF;
	int green = (value >> 8) & 0xFF;
	int red = (value >> 16) & 0xFF;

	return (blue == 127 && green == 188 && red == 217);
}

int	get_length(u32 value)
{
	int blue = value & 0xFF;
	int red = (value >> 16) & 0xFF;

	return (blue + red);
}

void	translate(u32 value, int length)
{
	char blue = value & 0xFF;
	char green = (value >> 8) & 0xFF;
	char red = (value >> 16) & 0xFF;

	if (length >= 3)
	{
		write(1, &blue, 1);
		write(1, &green, 1);
		write(1, &red, 1);
	}
	else if (length == 2)
	{
		write(1, &blue, 1);
		write(1, &green, 1);
	}
	else if (length == 1)
	{
		write(1, &blue, 1);
	}
}

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		PRINT_ERROR("Usage: decode <input_filename>\n");
		return 1;
	}
	struct file_content file_content = read_entire_file(argv[1]);
	if (file_content.data == NULL)
	{
		PRINT_ERROR("Failed to read file\n");
		return 1;
	}
	struct bmp_header* header = (struct bmp_header*) file_content.data;
	// printf("signature: %.2s\nfile_size: %u\ndata_offset: %u\ninfo_header_size: %u\nwidth: %u\nheight: %u\nplanes: %i\nbit_per_px: %i\ncompression_type: %u\ncompression_size: %u\n", header->signature, header->file_size, header->data_offset, header->info_header_size, header->width, header->height, header->number_of_planes, header->bit_per_pixel, header->compression_type, header->compressed_image_size);
	
	u32 i;
	i = header->data_offset;
	int header_count = 0;
	int	message_length = -1;

	while (i + 3 < file_content.size)
	{
		__m128i data = _mm_loadu_si128((__m128i*)&file_content.data[i]);
		u32 pixel = _mm_extract_epi32(data, 0);
		if (message_length == 0)
		{
			message_length = get_length(pixel);
			break;
		}
		if (is_header(pixel))
		{
			header_count++;
			if (header_count == 14)
				message_length = 0;
		}
		i += 4;
	}
	i -= 4 * 5;
	i -= (header->width * 4) * 2;
	int	pixels_to_read = message_length / 3;
	int	column = 0;
	for (int x = 0; x <= pixels_to_read; x++)
	{
		__m128i data = _mm_loadu_si128((__m128i*)&file_content.data[i]);
		u32 pixel = _mm_extract_epi32(data, 0);
		translate(pixel, message_length - (x * 3));
		column++;
		if (column == 6)
		{
			i -= 4 * 6;
			i -= (header->width * 4);
			column = 0;
		}
		i += 4;
	}
	printf("\n");
	// printf("len:%d\n", message_length);
	return 0;
}
