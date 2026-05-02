void SimpleModule::runPull()
{
	pull(m_outChunk->frameCount);
	run(*m_inChunk, *m_outChunk);
}