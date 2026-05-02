void jsP_dumpsyntax(js_State *J, js_Ast *prog, int dominify)
{
	minify = dominify;
	if (prog->type == AST_LIST)
		pstmlist(-1, prog);
	else {
		pstm(0, prog);
		nl();
	}
	if (minify > 1)
		putchar('\n');
}