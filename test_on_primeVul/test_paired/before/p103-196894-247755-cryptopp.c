Integer InvertibleRWFunction::CalculateInverse(RandomNumberGenerator &rng, const Integer &x) const
{
	DoQuickSanityCheck();
	ModularArithmetic modn(m_n);
	Integer r, rInv;
	do {	// do this in a loop for people using small numbers for testing
		r.Randomize(rng, Integer::One(), m_n - Integer::One());
		rInv = modn.MultiplicativeInverse(r);
	} while (rInv.IsZero());
	Integer re = modn.Square(r);
	re = modn.Multiply(re, x);			// blind

	Integer cp=re%m_p, cq=re%m_q;
	if (Jacobi(cp, m_p) * Jacobi(cq, m_q) != 1)
	{
		cp = cp.IsOdd() ? (cp+m_p) >> 1 : cp >> 1;
		cq = cq.IsOdd() ? (cq+m_q) >> 1 : cq >> 1;
	}

	#pragma omp parallel
		#pragma omp sections
		{
			#pragma omp section
				cp = ModularSquareRoot(cp, m_p);
			#pragma omp section
				cq = ModularSquareRoot(cq, m_q);
		}

	Integer y = CRT(cq, m_q, cp, m_p, m_u);
	y = modn.Multiply(y, rInv);				// unblind
	y = STDMIN(y, m_n-y);
	if (ApplyFunction(y) != x)				// check
		throw Exception(Exception::OTHER_ERROR, "InvertibleRWFunction: computational error during private key operation");
	return y;
}