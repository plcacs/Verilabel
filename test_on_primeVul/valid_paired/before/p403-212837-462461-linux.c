static int dw_spi_transfer_one(struct spi_controller *master,
		struct spi_device *spi, struct spi_transfer *transfer)
{
	struct dw_spi *dws = spi_controller_get_devdata(master);
	struct chip_data *chip = spi_get_ctldata(spi);
	u8 imask = 0;
	u16 txlevel = 0;
	u32 cr0;
	int ret;

	dws->dma_mapped = 0;

	dws->tx = (void *)transfer->tx_buf;
	dws->tx_end = dws->tx + transfer->len;
	dws->rx = transfer->rx_buf;
	dws->rx_end = dws->rx + transfer->len;
	dws->len = transfer->len;

	spi_enable_chip(dws, 0);

	/* Handle per transfer options for bpw and speed */
	if (transfer->speed_hz != dws->current_freq) {
		if (transfer->speed_hz != chip->speed_hz) {
			/* clk_div doesn't support odd number */
			chip->clk_div = (DIV_ROUND_UP(dws->max_freq, transfer->speed_hz) + 1) & 0xfffe;
			chip->speed_hz = transfer->speed_hz;
		}
		dws->current_freq = transfer->speed_hz;
		spi_set_clk(dws, chip->clk_div);
	}

	dws->n_bytes = DIV_ROUND_UP(transfer->bits_per_word, BITS_PER_BYTE);
	dws->dma_width = DIV_ROUND_UP(transfer->bits_per_word, BITS_PER_BYTE);

	/* Default SPI mode is SCPOL = 0, SCPH = 0 */
	cr0 = (transfer->bits_per_word - 1)
		| (chip->type << SPI_FRF_OFFSET)
		| ((((spi->mode & SPI_CPOL) ? 1 : 0) << SPI_SCOL_OFFSET) |
			(((spi->mode & SPI_CPHA) ? 1 : 0) << SPI_SCPH_OFFSET))
		| (chip->tmode << SPI_TMOD_OFFSET);

	/*
	 * Adjust transfer mode if necessary. Requires platform dependent
	 * chipselect mechanism.
	 */
	if (chip->cs_control) {
		if (dws->rx && dws->tx)
			chip->tmode = SPI_TMOD_TR;
		else if (dws->rx)
			chip->tmode = SPI_TMOD_RO;
		else
			chip->tmode = SPI_TMOD_TO;

		cr0 &= ~SPI_TMOD_MASK;
		cr0 |= (chip->tmode << SPI_TMOD_OFFSET);
	}

	dw_writel(dws, DW_SPI_CTRL0, cr0);

	/* Check if current transfer is a DMA transaction */
	if (master->can_dma && master->can_dma(master, spi, transfer))
		dws->dma_mapped = master->cur_msg_mapped;

	/* For poll mode just disable all interrupts */
	spi_mask_intr(dws, 0xff);

	/*
	 * Interrupt mode
	 * we only need set the TXEI IRQ, as TX/RX always happen syncronizely
	 */
	if (dws->dma_mapped) {
		ret = dws->dma_ops->dma_setup(dws, transfer);
		if (ret < 0) {
			spi_enable_chip(dws, 1);
			return ret;
		}
	} else if (!chip->poll_mode) {
		txlevel = min_t(u16, dws->fifo_len / 2, dws->len / dws->n_bytes);
		dw_writel(dws, DW_SPI_TXFLTR, txlevel);

		/* Set the interrupt mask */
		imask |= SPI_INT_TXEI | SPI_INT_TXOI |
			 SPI_INT_RXUI | SPI_INT_RXOI;
		spi_umask_intr(dws, imask);

		dws->transfer_handler = interrupt_transfer;
	}

	spi_enable_chip(dws, 1);

	if (dws->dma_mapped) {
		ret = dws->dma_ops->dma_transfer(dws, transfer);
		if (ret < 0)
			return ret;
	}

	if (chip->poll_mode)
		return poll_transfer(dws);

	return 1;
}