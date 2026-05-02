void png_print_chunk(unsigned type, unsigned char* data, unsigned size)
{
	char tag[5];
	unsigned i;

	be_uint32_write(tag, type);
	tag[4] = 0;

	cout << tag << setw(8) << size;

	switch (type) {
		case ADV_MNG_CN_MHDR :
			if (size < 28) {
				cout << " invalid chunk size";
				break;
			}
			cout << " width:" << be_uint32_read(data+0) << " height:" << be_uint32_read(data+4) << " frequency:" << be_uint32_read(data+8);
			cout << " simplicity:" << be_uint32_read(data+24);
			cout << "(bit";
			for(i=0;i<32;++i) {
				if (be_uint32_read(data+24) & (1 << i)) {
					cout << "," << i;
				}
			}
			cout << ")";
		break;
		case ADV_MNG_CN_DHDR :
			if (size < 4) {
				cout << " invalid chunk size";
				break;
			}
			cout << " id:" << be_uint16_read(data+0);
			switch (data[2]) {
				case 0 : cout << " img:unspecified"; break;
				case 1 : cout << " img:png"; break;
				case 2 : cout << " img:jng"; break;
				default: cout << " img:?"; break;
			}
			switch (data[3]) {
				case 0 : cout << " delta:entire_replacement"; break;
				case 1 : cout << " delta:block_addition"; break;
				case 2 : cout << " delta:block_alpha_addition"; break;
				case 3 : cout << " delta:block_color_addition"; break;
				case 4 : cout << " delta:block_replacement"; break;
				case 5 : cout << " delta:block_alpha_replacement"; break;
				case 6 : cout << " delta:block_color_replacement"; break;
				case 7 : cout << " delta:no_change"; break;
				default: cout << " delta:?"; break;
			}
			if (size >= 12) {
				cout << " width:" << be_uint32_read(data + 4) << " height:" << be_uint32_read(data + 8);
			}
			if (size >= 20) {
				cout << " x:" << (int)be_uint32_read(data + 12) << " y:" << (int)be_uint32_read(data + 16);
			}
		break;
		case ADV_MNG_CN_FRAM :
			if (size >= 1) {
				cout << " mode:" << (unsigned)data[0];
			}
			if (size > 1) {
				i = 1;
				while (i < size && data[i]!=0)
					++i;
				cout << " len:" << i-1;

				if (size >= i+2) {
					cout << " delay_mode:" << (unsigned)data[i+1];
				}

				if (size >= i+3) {
					cout << " timeout:" << (unsigned)data[i+2];
				}

				if (size >= i+4) {
					cout << " clip:" << (unsigned)data[i+3];
				}

				if (size >= i+5) {
					cout << " syncid:" << (unsigned)data[i+4];
				}

				if (size >= i+9) {
					cout << " tick:" << be_uint32_read(data+i+5);
				}

				if (size >= i+13) {
					cout << " timeout:" << be_uint32_read(data+i+9);
				}

				if (size >= i+14) {
					cout << " dt:" << (unsigned)data[i+10];
				}

				if (size >= i+15) {
					cout << " ...";
				}
			}
			break;
		case ADV_MNG_CN_DEFI :
			if (size < 2) {
				cout << " invalid chunk size";
				break;
			}
			cout << " id:" << be_uint16_read(data+0);
			if (size >= 3) {
				switch (data[2]) {
					case 0 : cout << " visible:yes"; break;
					case 1 : cout << " visible:no"; break;
					default : cout << " visible:?"; break;
				}
			}
			if (size >= 4) {
				switch (data[3]) {
					case 0 : cout << " concrete:abstract"; break;
					case 1 : cout << " concrete:concrete"; break;
					default : cout << " concrete:?"; break;
				}
			}
			if (size >= 12) {
				cout << " x:" << (int)be_uint32_read(data + 4) << " y:" << (int)be_uint32_read(data + 8);
			}
			if (size >= 28) {
				cout << " left:" << be_uint32_read(data + 12) << " right:" << be_uint32_read(data + 16) << " top:" << be_uint32_read(data + 20) << " bottom:" << be_uint32_read(data + 24);
			}
		break;
		case ADV_MNG_CN_MOVE :
			if (size < 13) {
				cout << " invalid chunk size";
				break;
			}
			cout << " id_from:" << be_uint16_read(data+0) << " id_to:" << be_uint16_read(data+2);
			switch (data[4]) {
				case 0 : cout << " type:replace"; break;
				case 1 : cout << " type:add"; break;
				default : cout << " type:?"; break;
			}
			cout << " x:" << (int)be_uint32_read(data + 5) << " y:" << (int)be_uint32_read(data + 9);
			break;
		case ADV_MNG_CN_PPLT :
			if (size < 1) {
				cout << " invalid chunk size";
				break;
			}
			switch (data[0]) {
				case 0 : cout << " type:replacement_rgb"; break;
				case 1 : cout << " type:delta_rgb"; break;
				case 2 : cout << " type:replacement_alpha"; break;
				case 3 : cout << " type:delta_alpha"; break;
				case 4 : cout << " type:replacement_rgba"; break;
				case 5 : cout << " type:delta_rgba"; break;
				default : cout << " type:?"; break;
			}
			i = 1;
			while (i + 1 < size) {
				unsigned ssize;
				cout << " " << (unsigned)data[i] << ":" << (unsigned)data[i+1];
				if (data[0] == 0 || data[1] == 1)
					ssize = 3;
				else if (data[0] == 2 || data[1] == 3)
					ssize = 1;
				else
					ssize = 4;
				i += 2 + (data[i+1] - data[i] + 1) * ssize;
			}
			break;
		case ADV_PNG_CN_IHDR :
			if (size < 13) {
				cout << " invalid chunk size";
				break;
			}
			cout << " width:" << be_uint32_read(data) << " height:" << be_uint32_read(data + 4);
			cout << " depth:" << (unsigned)data[8];
			cout << " color_type:" << (unsigned)data[9];
			cout << " compression:" << (unsigned)data[10];
			cout << " filter:" << (unsigned)data[11];
			cout << " interlace:" << (unsigned)data[12];
		break;
	}

	cout << endl;
}