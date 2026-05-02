int hw_atl_utils_fw_rpc_wait(struct aq_hw_s *self,
			     struct hw_atl_utils_fw_rpc **rpc)
{
	struct aq_hw_atl_utils_fw_rpc_tid_s sw;
	struct aq_hw_atl_utils_fw_rpc_tid_s fw;
	int err = 0;

	do {
		sw.val = aq_hw_read_reg(self, HW_ATL_RPC_CONTROL_ADR);

		self->rpc_tid = sw.tid;

		err = readx_poll_timeout_atomic(hw_atl_utils_rpc_state_get,
						self, fw.val,
						sw.tid == fw.tid,
						1000U, 100000U);
		if (err < 0)
			goto err_exit;

		err = aq_hw_err_from_flags(self);
		if (err < 0)
			goto err_exit;

		if (fw.len == 0xFFFFU) {
			err = hw_atl_utils_fw_rpc_call(self, sw.len);
			if (err < 0)
				goto err_exit;
		}
	} while (sw.tid != fw.tid || 0xFFFFU == fw.len);

	if (rpc) {
		if (fw.len) {
			err =
			hw_atl_utils_fw_downld_dwords(self,
						      self->rpc_addr,
						      (u32 *)(void *)
						      &self->rpc,
						      (fw.len + sizeof(u32) -
						       sizeof(u8)) /
						      sizeof(u32));
			if (err < 0)
				goto err_exit;
		}

		*rpc = &self->rpc;
	}

err_exit:
	return err;
}