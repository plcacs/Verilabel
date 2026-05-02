static int ismt_access(struct i2c_adapter *adap, u16 addr,
		       unsigned short flags, char read_write, u8 command,
		       int size, union i2c_smbus_data *data)
{
	int ret;
	unsigned long time_left;
	dma_addr_t dma_addr = 0; /* address of the data buffer */
	u8 dma_size = 0;
	enum dma_data_direction dma_direction = 0;
	struct ismt_desc *desc;
	struct ismt_priv *priv = i2c_get_adapdata(adap);
	struct device *dev = &priv->pci_dev->dev;
	u8 *dma_buffer = PTR_ALIGN(&priv->buffer[0], 16);

	desc = &priv->hw[priv->head];

	/* Initialize the DMA buffer */
	memset(priv->buffer, 0, sizeof(priv->buffer));

	/* Initialize the descriptor */
	memset(desc, 0, sizeof(struct ismt_desc));
	desc->tgtaddr_rw = ISMT_DESC_ADDR_RW(addr, read_write);

	/* Always clear the log entries */
	memset(priv->log, 0, ISMT_LOG_ENTRIES * sizeof(u32));

	/* Initialize common control bits */
	if (likely(pci_dev_msi_enabled(priv->pci_dev)))
		desc->control = ISMT_DESC_INT | ISMT_DESC_FAIR;
	else
		desc->control = ISMT_DESC_FAIR;

	if ((flags & I2C_CLIENT_PEC) && (size != I2C_SMBUS_QUICK)
	    && (size != I2C_SMBUS_I2C_BLOCK_DATA))
		desc->control |= ISMT_DESC_PEC;

	switch (size) {
	case I2C_SMBUS_QUICK:
		dev_dbg(dev, "I2C_SMBUS_QUICK\n");
		break;

	case I2C_SMBUS_BYTE:
		if (read_write == I2C_SMBUS_WRITE) {
			/*
			 * Send Byte
			 * The command field contains the write data
			 */
			dev_dbg(dev, "I2C_SMBUS_BYTE:  WRITE\n");
			desc->control |= ISMT_DESC_CWRL;
			desc->wr_len_cmd = command;
		} else {
			/* Receive Byte */
			dev_dbg(dev, "I2C_SMBUS_BYTE:  READ\n");
			dma_size = 1;
			dma_direction = DMA_FROM_DEVICE;
			desc->rd_len = 1;
		}
		break;

	case I2C_SMBUS_BYTE_DATA:
		if (read_write == I2C_SMBUS_WRITE) {
			/*
			 * Write Byte
			 * Command plus 1 data byte
			 */
			dev_dbg(dev, "I2C_SMBUS_BYTE_DATA:  WRITE\n");
			desc->wr_len_cmd = 2;
			dma_size = 2;
			dma_direction = DMA_TO_DEVICE;
			dma_buffer[0] = command;
			dma_buffer[1] = data->byte;
		} else {
			/* Read Byte */
			dev_dbg(dev, "I2C_SMBUS_BYTE_DATA:  READ\n");
			desc->control |= ISMT_DESC_CWRL;
			desc->wr_len_cmd = command;
			desc->rd_len = 1;
			dma_size = 1;
			dma_direction = DMA_FROM_DEVICE;
		}
		break;

	case I2C_SMBUS_WORD_DATA:
		if (read_write == I2C_SMBUS_WRITE) {
			/* Write Word */
			dev_dbg(dev, "I2C_SMBUS_WORD_DATA:  WRITE\n");
			desc->wr_len_cmd = 3;
			dma_size = 3;
			dma_direction = DMA_TO_DEVICE;
			dma_buffer[0] = command;
			dma_buffer[1] = data->word & 0xff;
			dma_buffer[2] = data->word >> 8;
		} else {
			/* Read Word */
			dev_dbg(dev, "I2C_SMBUS_WORD_DATA:  READ\n");
			desc->wr_len_cmd = command;
			desc->control |= ISMT_DESC_CWRL;
			desc->rd_len = 2;
			dma_size = 2;
			dma_direction = DMA_FROM_DEVICE;
		}
		break;

	case I2C_SMBUS_PROC_CALL:
		dev_dbg(dev, "I2C_SMBUS_PROC_CALL\n");
		desc->wr_len_cmd = 3;
		desc->rd_len = 2;
		dma_size = 3;
		dma_direction = DMA_BIDIRECTIONAL;
		dma_buffer[0] = command;
		dma_buffer[1] = data->word & 0xff;
		dma_buffer[2] = data->word >> 8;
		break;

	case I2C_SMBUS_BLOCK_DATA:
		if (read_write == I2C_SMBUS_WRITE) {
			/* Block Write */
			dev_dbg(dev, "I2C_SMBUS_BLOCK_DATA:  WRITE\n");
			dma_size = data->block[0] + 1;
			dma_direction = DMA_TO_DEVICE;
			desc->wr_len_cmd = dma_size;
			desc->control |= ISMT_DESC_BLK;
			dma_buffer[0] = command;
			memcpy(&dma_buffer[1], &data->block[1], dma_size - 1);
		} else {
			/* Block Read */
			dev_dbg(dev, "I2C_SMBUS_BLOCK_DATA:  READ\n");
			dma_size = I2C_SMBUS_BLOCK_MAX;
			dma_direction = DMA_FROM_DEVICE;
			desc->rd_len = dma_size;
			desc->wr_len_cmd = command;
			desc->control |= (ISMT_DESC_BLK | ISMT_DESC_CWRL);
		}
		break;

	case I2C_SMBUS_BLOCK_PROC_CALL:
		dev_dbg(dev, "I2C_SMBUS_BLOCK_PROC_CALL\n");
		dma_size = I2C_SMBUS_BLOCK_MAX;
		desc->tgtaddr_rw = ISMT_DESC_ADDR_RW(addr, 1);
		desc->wr_len_cmd = data->block[0] + 1;
		desc->rd_len = dma_size;
		desc->control |= ISMT_DESC_BLK;
		dma_direction = DMA_BIDIRECTIONAL;
		dma_buffer[0] = command;
		memcpy(&dma_buffer[1], &data->block[1], data->block[0]);
		break;

	case I2C_SMBUS_I2C_BLOCK_DATA:
		/* Make sure the length is valid */
		if (data->block[0] < 1)
			data->block[0] = 1;

		if (data->block[0] > I2C_SMBUS_BLOCK_MAX)
			data->block[0] = I2C_SMBUS_BLOCK_MAX;

		if (read_write == I2C_SMBUS_WRITE) {
			/* i2c Block Write */
			dev_dbg(dev, "I2C_SMBUS_I2C_BLOCK_DATA:  WRITE\n");
			dma_size = data->block[0] + 1;
			dma_direction = DMA_TO_DEVICE;
			desc->wr_len_cmd = dma_size;
			desc->control |= ISMT_DESC_I2C;
			dma_buffer[0] = command;
			memcpy(&dma_buffer[1], &data->block[1], dma_size - 1);
		} else {
			/* i2c Block Read */
			dev_dbg(dev, "I2C_SMBUS_I2C_BLOCK_DATA:  READ\n");
			dma_size = data->block[0];
			dma_direction = DMA_FROM_DEVICE;
			desc->rd_len = dma_size;
			desc->wr_len_cmd = command;
			desc->control |= (ISMT_DESC_I2C | ISMT_DESC_CWRL);
			/*
			 * Per the "Table 15-15. I2C Commands",
			 * in the External Design Specification (EDS),
			 * (Document Number: 508084, Revision: 2.0),
			 * the _rw bit must be 0
			 */
			desc->tgtaddr_rw = ISMT_DESC_ADDR_RW(addr, 0);
		}
		break;

	default:
		dev_err(dev, "Unsupported transaction %d\n",
			size);
		return -EOPNOTSUPP;
	}

	/* map the data buffer */
	if (dma_size != 0) {
		dev_dbg(dev, " dev=%p\n", dev);
		dev_dbg(dev, " data=%p\n", data);
		dev_dbg(dev, " dma_buffer=%p\n", dma_buffer);
		dev_dbg(dev, " dma_size=%d\n", dma_size);
		dev_dbg(dev, " dma_direction=%d\n", dma_direction);

		dma_addr = dma_map_single(dev,
				      dma_buffer,
				      dma_size,
				      dma_direction);

		if (dma_mapping_error(dev, dma_addr)) {
			dev_err(dev, "Error in mapping dma buffer %p\n",
				dma_buffer);
			return -EIO;
		}

		dev_dbg(dev, " dma_addr = %pad\n", &dma_addr);

		desc->dptr_low = lower_32_bits(dma_addr);
		desc->dptr_high = upper_32_bits(dma_addr);
	}

	reinit_completion(&priv->cmp);

	/* Add the descriptor */
	ismt_submit_desc(priv);

	/* Now we wait for interrupt completion, 1s */
	time_left = wait_for_completion_timeout(&priv->cmp, HZ*1);

	/* unmap the data buffer */
	if (dma_size != 0)
		dma_unmap_single(dev, dma_addr, dma_size, dma_direction);

	if (unlikely(!time_left)) {
		dev_err(dev, "completion wait timed out\n");
		ret = -ETIMEDOUT;
		goto out;
	}

	/* do any post processing of the descriptor here */
	ret = ismt_process_desc(desc, data, priv, size, read_write);

out:
	/* Update the ring pointer */
	priv->head++;
	priv->head %= ISMT_DESC_ENTRIES;

	return ret;
}