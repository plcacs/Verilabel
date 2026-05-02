void SimpleModule::runPull()
{
	pull(m_outChunk->frameCount);
	m_outChunk->frameCount = m_inChunk->frameCount;
	run(*m_inChunk, *m_outChunk);
}