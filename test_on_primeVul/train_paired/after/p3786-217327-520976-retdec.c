bool Decoder::canSplitFunctionOn(
		utils::Address addr,
		llvm::BasicBlock* splitBb,
		std::set<llvm::BasicBlock*>& newFncStarts)
{
	newFncStarts.insert(splitBb);

	auto* f = splitBb->getParent();
	auto fAddr = getFunctionAddress(f);

	auto fSzIt = _fnc2sz.find(f);
	if (fSzIt != _fnc2sz.end())
	{
		if (fAddr <= addr && addr < (fAddr+fSzIt->second))
		{
			LOG << "\t\t\t\t\t" << "!CAN S: addr cond @ " << addr << std::endl;
			return false;
		}
	}

	std::set<Address> fncStarts;
	fncStarts.insert(fAddr);
	fncStarts.insert(addr);

	LOG << "\t\t\t\t\t" << "CAN S: split @ " << fAddr << std::endl;
	LOG << "\t\t\t\t\t" << "CAN S: split @ " << addr << std::endl;

	bool changed = true;
	while (changed)
	{
		changed = false;
		for (BasicBlock& b : *f)
		{
//			Address bAddr = getBasicBlockAddress(&b);
			Address bAddr;
			// TODO: shitty
			BasicBlock* bPrev = &b;
			while (bAddr.isUndefined() && bPrev)
			{
				bAddr = getBasicBlockAddress(bPrev);
				bPrev = bPrev->getPrevNode();
			}
			if (bAddr.isUndefined())
			{
				continue;
			}
			auto up = fncStarts.upper_bound(bAddr);
			if (up == fncStarts.begin()) {
				return false;
			}	
			--up;
			Address bFnc = *up;

			for (auto* p : predecessors(&b))
			{
//				Address pAddr = getBasicBlockAddress(p);
				Address pAddr;
				// TODO: shitty
				BasicBlock* pPrev = p;
				while (pAddr.isUndefined() && pPrev)
				{
					pAddr = getBasicBlockAddress(pPrev);
					pPrev = pPrev->getPrevNode();
				}
				if (pAddr.isUndefined())
				{
					continue;
				}
				auto up = fncStarts.upper_bound(pAddr);
				if (up == fncStarts.begin()) {
					return false;
				}
				--up;
				Address pFnc = *up;

				if (bFnc != pFnc)
				{
					if (!canSplitFunctionOn(&b))
					{
						return false;
					}

					changed |= newFncStarts.insert(&b).second;
					changed |= fncStarts.insert(bAddr).second;

					LOG << "\t\t\t\t\t" << "CAN S: split @ " << bAddr << std::endl;
				}
			}
		}
	}

	return true;
}