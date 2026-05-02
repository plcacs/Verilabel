static bool parseOperands(char* str, ArmOp *op) {
	char *t = strdup (str);
	int operand = 0;
	char *token = t;
	char *x;
	int imm_count = 0;
	int mem_opt = 0;
	if (!token) {
		return false;
	}

	while (token) {
		char *next = strchr (token, ',');
		if (next) {
			*next++ = 0;
		}
		while (token[0] == ' ') {
			token++;
		}
		if (operand >= MAX_OPERANDS) {
			eprintf ("Too many operands\n");
			return false;
		}
		op->operands[operand].type = ARM_NOTYPE;
		op->operands[operand].reg_type = ARM_UNDEFINED;
		op->operands[operand].shift = ARM_NO_SHIFT;

		while (token[0] == ' ' || token[0] == '[' || token[0] == ']') {
			token ++;
		}

		if (!strncmp (token, "lsl", 3)) {
			op->operands[operand].shift = ARM_LSL;
		} else if (!strncmp (token, "lsr", 3)) {
			op->operands[operand].shift = ARM_LSR;
		} else if (!strncmp (token, "asr", 3)) {
			op->operands[operand].shift = ARM_ASR;
		}
		if (op->operands[operand].shift != ARM_NO_SHIFT) {
			op->operands_count ++;
			op->operands[operand].shift_amount = r_num_math (NULL, token + 4);
			if (op->operands[operand].shift_amount > 63) {
				return false;
			}
			operand ++;
			token = next;
			continue;
		}

		switch (token[0]) {
		case 'x':
			x = strchr (token, ',');
			if (x) {
				x[0] = '\0';
			}
			op->operands_count ++;
			op->operands[operand].type = ARM_GPR;
			op->operands[operand].reg_type = ARM_REG64;
			op->operands[operand].reg = r_num_math (NULL, token + 1);
			if (op->operands[operand].reg > 31) {
				return false;
			}
			break;
		case 'w':
			op->operands_count ++;
			op->operands[operand].type = ARM_GPR;
			op->operands[operand].reg_type = ARM_REG32;
			op->operands[operand].reg = r_num_math (NULL, token + 1);
			if (op->operands[operand].reg > 31) {
				return false;
			}
			break;
		case 'v':
			op->operands_count ++;
			op->operands[operand].type = ARM_FP;
			op->operands[operand].reg = r_num_math (NULL, token + 1);
			break;
		case 's':
		case 'S':
			if (token[1] == 'P' || token [1] == 'p') {
				int i;
				for (i = 0; msr_const[i].name; i++) {
					if (!r_str_ncasecmp (token, msr_const[i].name, strlen (msr_const[i].name))) {
						op->operands[operand].sp_val = msr_const[i].val;
						break;
					}
				}
				op->operands_count ++;
				op->operands[operand].type = ARM_GPR;
				op->operands[operand].reg_type = ARM_SP | ARM_REG64;
				op->operands[operand].reg = 31;
				break;
			}
			mem_opt = get_mem_option (token);
			if (mem_opt != -1) {
				op->operands_count ++;
				op->operands[operand].type = ARM_MEM_OPT;
				op->operands[operand].mem_option = mem_opt;
			}
			break;
		case 'L':
		case 'l':
		case 'I':
		case 'i':
		case 'N':
		case 'n':
		case 'O':
		case 'o':
		case 'p':
		case 'P':
			mem_opt = get_mem_option (token);
			if (mem_opt != -1) {
				op->operands_count ++;
				op->operands[operand].type = ARM_MEM_OPT;
				op->operands[operand].mem_option = mem_opt;
			}
			break;
		case '-':
			op->operands[operand].sign = -1;
			// falthru
		default:
			op->operands_count ++;
			op->operands[operand].type = ARM_CONSTANT;
			op->operands[operand].immediate = r_num_math (NULL, token);
			imm_count++;
			break;
		}
		token = next;

		operand ++;
		if (operand > MAX_OPERANDS) {
			free (t);
			return false;
		}
	}
	free (t);
	return true;
}