
#include <stdio.h>
#include <iostream>		// for system("pause")
extern "C"				// third-party C-libraries (not C++) are not included easily in Visual Studio =)
{
	#include "crc.h"
}

#define T2_HEADER	0
#define OTHER		1


// data types
struct ts_packet_info
{
	unsigned int PID;
	bool PUSI;	// payload unit start indicator
	unsigned char pointer_value;
	bool AF;	// adaptation field flag
	unsigned char AF_len;
	unsigned char header_len;	// including AF and pointer
	unsigned int header;
};
struct t2_frame_info
{
	unsigned char type;
	unsigned char pkt_count;
	unsigned char sframe_idx;
	unsigned char stream_id;
	unsigned short payload_len;
};
struct bb_frame_info
{
	unsigned char frame_idx;
	unsigned char PLP_id;
	bool IL_frame_start;
	unsigned short dfl;
	unsigned short syncd;
	unsigned char crc8_xor_mode;
	unsigned char calculated_crc8;
	bool calculated_mode;			// NM = 0, HEM = 1
	unsigned char upl;
};
struct L1_info
{
	unsigned char plp_num_blocks;
	bool plp_mode;			// NM = 0, HEM = 1
};

// global vars
ts_packet_info current_tspacket_info;
t2_frame_info current_t2frame_info;
bb_frame_info current_bbframe_info;
L1_info current_L1_info;
unsigned char ts_byte_counter;
unsigned long long packet_counter;
bool first_accumulation;
bool inner_ts_start_found;
unsigned char actual_syncd;

void SendToFile(FILE* file, unsigned char* array, unsigned short len)
{
	if (current_bbframe_info.syncd != actual_syncd)
		printf("ts pkt = %d; syncd = %d; actual offset = %d\n", packet_counter, current_bbframe_info.syncd, actual_syncd);
	unsigned char m = 0;
	unsigned short i = 0;
	unsigned char sync_byte = 0x47;
	unsigned char* p_sync_byte = &sync_byte;
	if (!inner_ts_start_found)
	{
		i += current_bbframe_info.syncd;
		inner_ts_start_found = true;
	}

	if (actual_syncd != 0)
		goto INSERT_PAYLOAD;
	
	INSERT_SYNC:
	fwrite(p_sync_byte, 1, 1, file);

	INSERT_PAYLOAD:
	if (actual_syncd != 0)
	{
		m = actual_syncd;
		actual_syncd = 0;
	}
	else if ((len - i) > 187)
		m = 187;
	else
		m = len - i;
	fwrite((array + i), 1, m, file);
	i += m;

	if (i < len)
		goto INSERT_SYNC;
	else
		actual_syncd = 187 - m;
	
}

unsigned int Array2Int(unsigned char* array, unsigned char n)	// max n = 4;
{
	unsigned int var_int = 0;						// создаём переменную (выделяем место в памяти)
	unsigned int* p_int = &var_int;					// создаём указатель на предыдущую переменную
	unsigned char* p_byte = (unsigned char*)p_int;	// преобразуем указатель на int в указатель на byte
	for (unsigned char i = 0; i<n; i++)				// в цикле заполняем байты int'а (в массиве порядок реверсивный)
		p_byte[i] = array[n - i - 1];
	return var_int;
}

unsigned int ReadBytes(FILE* fp, unsigned char n)	// max n = 4
{
	unsigned int my_integer = 0;
	unsigned int* p_my_integer = &my_integer;
	char* pbyte = (char*)p_my_integer;
	for (char i = (n - 1); i >= 0; i--)
	{
		if (feof(fp))	// experimental break
		{
			printf("End of file\n");
			return 0;
		}
		*(pbyte + i) = fgetc(fp);	// функция fgetc помимо забора значения также перемещает указатель на 1 байт вперёд
		ts_byte_counter++;
	}
	return my_integer;
}

unsigned int ExtractBits(unsigned int number_from, char start_bit, char end_bit)
{
	char num_of_bits = start_bit - end_bit + 1;
	unsigned int mask_number = (1 << num_of_bits) - 1;
	unsigned int extracted_number = (number_from >> end_bit) & mask_number;
	return extracted_number;
}

ts_packet_info ParseTSHeader(FILE* fp)
{
	unsigned int header = ReadBytes(fp, 4);
	ts_packet_info var_tspacket_info;
	var_tspacket_info.pointer_value = 0;
	var_tspacket_info.PID = ExtractBits(header, 20, 8);
	var_tspacket_info.PUSI = ExtractBits(header, 22, 22);
	char AF = ExtractBits(header, 5, 4);
	if (AF >= 2)
		var_tspacket_info.AF = true;
	else
		var_tspacket_info.AF = false;

	var_tspacket_info.header_len = 4;

	if (var_tspacket_info.AF == true)
	{
		var_tspacket_info.AF_len = fgetc(fp); ts_byte_counter++;
		var_tspacket_info.header_len = var_tspacket_info.header_len + 1 + var_tspacket_info.AF_len;
		fseek(fp, var_tspacket_info.AF_len, SEEK_CUR); ts_byte_counter += var_tspacket_info.AF_len;
	}
	if (var_tspacket_info.PUSI == true)
	{
		var_tspacket_info.pointer_value = fgetc(fp); ts_byte_counter++;
		var_tspacket_info.header_len = var_tspacket_info.header_len + 1;
	}

	var_tspacket_info.header = header;

	return var_tspacket_info;	
}

unsigned char* AccumulateBytes(FILE* fp, unsigned short n, int flag)	// Accumulates n bytes into array, skipping TS headers
{
	if((flag == T2_HEADER) && (first_accumulation == true))
		first_accumulation = false;
	unsigned short i = 0;
	unsigned char* array = new unsigned char[n];
	LOOP_1:
	if (ts_byte_counter == 0)
	{
		LOOP_2:
		packet_counter++;
		current_tspacket_info = ParseTSHeader(fp);
		if (current_tspacket_info.header == 0)
			return array;

		if (current_tspacket_info.PID != 0x1000)
		{
			fseek(fp, (188 - current_tspacket_info.header_len), SEEK_CUR); ts_byte_counter = 0;
			goto LOOP_2;
		}

		first_accumulation = true;
	}

	array[i] = fgetc(fp);
	if (ts_byte_counter < 187)
		ts_byte_counter++;
	else
		ts_byte_counter = 0;
	i++;
	if (i < n)
		goto LOOP_1;
	return array;
}

void FindFirstT2Frame(FILE* fp)
{
	LOOP:
	packet_counter++;
	current_tspacket_info = ParseTSHeader(fp);
	if((current_tspacket_info.PID != 0x1000) || (current_tspacket_info.PUSI == false))
	{
		fseek(fp, (188 - current_tspacket_info.header_len), SEEK_CUR); ts_byte_counter = 0;
		goto LOOP;
	}
	fseek(fp, current_tspacket_info.pointer_value, SEEK_CUR);
	ts_byte_counter += current_tspacket_info.pointer_value;
}

t2_frame_info ParseT2Header(unsigned char* header)	// 6 bytes
{
	t2_frame_info var_t2frame_info;
	var_t2frame_info.type = header[0];
	var_t2frame_info.pkt_count = header[1];
	var_t2frame_info.sframe_idx = ExtractBits(header[2], 7, 4);
	var_t2frame_info.stream_id = ExtractBits(header[3], 2, 0);
	var_t2frame_info.payload_len = ((header[4] << 8) + header[5]) >> 3;	// in bytes
	delete[] header;
	return var_t2frame_info;
}

bb_frame_info ParseBBHeader(unsigned char* header)	// 3 + 10 bytes
{
	bb_frame_info var_bbframe_info;
	var_bbframe_info.frame_idx = header[0];
	var_bbframe_info.PLP_id = header[1];
	var_bbframe_info.IL_frame_start = ExtractBits(header[2], 7, 7);
	var_bbframe_info.dfl = ((header[7] << 8) + header[8]) >> 3;	// in bytes
	var_bbframe_info.syncd = ((header[10] << 8) + header[11]) >> 3;	// in bytes
	var_bbframe_info.crc8_xor_mode = header[12];
	var_bbframe_info.calculated_crc8 = crc8Slow((header + 3), 9, 0);
	if ((var_bbframe_info.crc8_xor_mode & 0xFE) != (var_bbframe_info.calculated_crc8 & 0xFE))
		printf("ts pkt = %d; in BB-frame: crc8^mode = %02x, calculated crc8 = %02x\n",packet_counter, var_bbframe_info.crc8_xor_mode, var_bbframe_info.calculated_crc8);
	var_bbframe_info.calculated_mode = var_bbframe_info.crc8_xor_mode ^ var_bbframe_info.calculated_crc8;

	if (var_bbframe_info.calculated_mode == false)
		var_bbframe_info.upl = ((header[5] << 8) + header[6]) >> 3;
	else
		var_bbframe_info.upl = 187;
	delete[] header;
	return var_bbframe_info;
}

L1_info ParseL1(unsigned char* L1)
{
	L1_info var_L1_info;
	var_L1_info.plp_num_blocks = (ExtractBits(L1[63], 2, 0) << 7) | (ExtractBits(L1[64], 7, 1));
	var_L1_info.plp_mode = ExtractBits(L1[44], 4, 4);	// 0 = HEM, 1 = NM

	return var_L1_info;
}

void RemoveExtension(char* filename)
{
	unsigned char i = 0;
	while (filename[i] != '.')
		i++;
	filename[i] = '\0';
}


int main()
{
	FILE* fp;
	printf("Input file name (with extension, max 99 symbols):\n");
	char* input_string;
	input_string = new char[100];
	scanf("%s", input_string);
	if ((fp = fopen(input_string, "rb")) == NULL)
	{
		printf("Couldn't open file\n");
		system("pause");
		return 0;
	}
	printf("\nFile found. Processing...\n\n");
	RemoveExtension(input_string);
	char* output_filename = new char[strlen(input_string) + 12];
	char* inner_ts_filename = new char[strlen(input_string) + 10];
	strcpy(output_filename, input_string); strcat(output_filename, "_output.txt");
	strcpy(inner_ts_filename, input_string); strcat(inner_ts_filename, "_inner.ts");
	FILE* fout = fopen(output_filename, "wt");
	FILE* f_inner = fopen(inner_ts_filename,"wb");
	delete[] input_string;
	delete[] output_filename;
	delete[] inner_ts_filename;

	fprintf(fout, "      # type pkt_count sframe_idx stream_id payl_len frame_idx PLP_ID #_BB ILFS   DFL CRC8^mode calc'd_CRC32    CRC32 errors\n\n");

	ts_byte_counter = 0;
	packet_counter = 0;
	first_accumulation = false;
	inner_ts_start_found = false;
	actual_syncd = 0;
	crc32 previous_crc;
	unsigned int crc_32;
	unsigned char previous_pkt_count = 0;
	unsigned char BBframe_num = 0;
	bool error_plp_num_blocks = false;
	
	FindFirstT2Frame(fp);

	LOOP:
	unsigned char* temp_array = AccumulateBytes(fp, 6, T2_HEADER);
	previous_crc = crc32Slow(temp_array, 6, 0xFFFFFFFF);
	current_t2frame_info = ParseT2Header(temp_array);

	fprintf(fout, "%7d ", packet_counter);
	fprintf(fout, "  %02x ", current_t2frame_info.type);
	fprintf(fout, "       %02x ", current_t2frame_info.pkt_count);
	fprintf(fout, "         %x ", current_t2frame_info.sframe_idx);
	fprintf(fout, "        %x ", current_t2frame_info.stream_id);
	fprintf(fout, "%8d ", current_t2frame_info.payload_len);

	if (current_t2frame_info.type == 0x00)
	{
		temp_array = AccumulateBytes(fp, 13, OTHER);
		previous_crc = crc32Slow(temp_array, 13, previous_crc);
		current_bbframe_info = ParseBBHeader(temp_array);

		fprintf(fout, "       %02x ", current_bbframe_info.frame_idx);
		fprintf(fout, "    %02x ", current_bbframe_info.PLP_id);
		if (current_bbframe_info.IL_frame_start == true)
		{
			if (BBframe_num != (current_L1_info.plp_num_blocks - 1))
				error_plp_num_blocks = true;
			BBframe_num = 0;
		}
		else
			BBframe_num++;
		fprintf(fout, "%4d ", BBframe_num);
		fprintf(fout, "   %d ", current_bbframe_info.IL_frame_start);
		fprintf(fout, "%5d ", current_bbframe_info.dfl);
		fprintf(fout, "       %02x ", current_bbframe_info.crc8_xor_mode);

		temp_array = AccumulateBytes(fp, current_bbframe_info.dfl, OTHER);
		SendToFile(f_inner, temp_array, current_bbframe_info.dfl);
		previous_crc = crc32Slow(temp_array, current_bbframe_info.dfl, previous_crc);
		fprintf(fout, "    %08x ", previous_crc);
		delete[] temp_array;
	}
	else if (	(current_t2frame_info.type == 0x20) ||
				(current_t2frame_info.type == 0x10) ||
				(current_t2frame_info.type == 0x11)		)
	{
		temp_array = AccumulateBytes(fp, current_t2frame_info.payload_len, OTHER);

		if (current_t2frame_info.type == 0x10)
			current_L1_info = ParseL1(temp_array);

		previous_crc = crc32Slow(temp_array, current_t2frame_info.payload_len, previous_crc);
		fprintf(fout, "                                               %08x ", previous_crc);
		delete[] temp_array;
	}
	else
	{
		fprintf(fout, "\nUnknown packet type");
		return 0;
	}

	temp_array = AccumulateBytes(fp, 4, OTHER);
	crc_32 = Array2Int(temp_array, 4);
	delete[] temp_array;
	fprintf(fout, "%08x ", crc_32);
	if (crc_32 != previous_crc)
		fprintf(fout, "crc32_error ");
	if (previous_pkt_count != (unsigned char)(current_t2frame_info.pkt_count - 1))
		fprintf(fout, "pkt_count error ");
	previous_pkt_count = current_t2frame_info.pkt_count;
	if (error_plp_num_blocks)
	{
		fprintf(fout, "plp_num_blocks error ");
		error_plp_num_blocks = false;
	}
	fprintf(fout, "\n");
	// check pointer_field
	if ((current_tspacket_info.PUSI == true) && (first_accumulation == true))
	{
		if (current_tspacket_info.pointer_value != (ts_byte_counter - current_tspacket_info.header_len))
			fprintf(fout, "For next T2-MI packet: PF = %d, actual offset = %d\n", current_tspacket_info.pointer_value, (ts_byte_counter - current_tspacket_info.header_len));
	}

	if (!feof(fp))
		goto LOOP;

	fclose(fp);
	fclose(fout);
	fclose(f_inner);
	system("pause");
	return 0;
}
