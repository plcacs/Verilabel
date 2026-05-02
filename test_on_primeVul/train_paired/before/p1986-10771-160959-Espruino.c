NO_INLINE JsVar *jspeFactor() {
  if (lex->tk==LEX_ID) {
    JsVar *a = jspGetNamedVariable(jslGetTokenValueAsString(lex));
    JSP_ASSERT_MATCH(LEX_ID);
#ifndef SAVE_ON_FLASH
    if (lex->tk==LEX_TEMPLATE_LITERAL)
      jsExceptionHere(JSET_SYNTAXERROR, "Tagged template literals not supported");
    else if (lex->tk==LEX_ARROW_FUNCTION && jsvIsName(a)) {
      JsVar *funcVar = jspeArrowFunction(0,a);
      jsvUnLock(a);
      a=funcVar;
    }
#endif
    return a;
  } else if (lex->tk==LEX_INT) {
    JsVar *v = 0;
    if (JSP_SHOULD_EXECUTE) {
      v = jsvNewFromLongInteger(stringToInt(jslGetTokenValueAsString(lex)));
    }
    JSP_ASSERT_MATCH(LEX_INT);
    return v;
  } else if (lex->tk==LEX_FLOAT) {
    JsVar *v = 0;
    if (JSP_SHOULD_EXECUTE) {
      v = jsvNewFromFloat(stringToFloat(jslGetTokenValueAsString(lex)));
    }
    JSP_ASSERT_MATCH(LEX_FLOAT);
    return v;
  } else if (lex->tk=='(') {
    JSP_ASSERT_MATCH('(');
    if (!jspCheckStackPosition()) return 0;
#ifdef SAVE_ON_FLASH
    // Just parse a normal expression (which can include commas)
    JsVar *a = jspeExpression();
    if (!JSP_SHOULDNT_PARSE) JSP_MATCH_WITH_RETURN(')',a);
    return a;
#else
    return jspeExpressionOrArrowFunction();
#endif

  } else if (lex->tk==LEX_R_TRUE) {
    JSP_ASSERT_MATCH(LEX_R_TRUE);
    return JSP_SHOULD_EXECUTE ? jsvNewFromBool(true) : 0;
  } else if (lex->tk==LEX_R_FALSE) {
    JSP_ASSERT_MATCH(LEX_R_FALSE);
    return JSP_SHOULD_EXECUTE ? jsvNewFromBool(false) : 0;
  } else if (lex->tk==LEX_R_NULL) {
    JSP_ASSERT_MATCH(LEX_R_NULL);
    return JSP_SHOULD_EXECUTE ? jsvNewWithFlags(JSV_NULL) : 0;
  } else if (lex->tk==LEX_R_UNDEFINED) {
    JSP_ASSERT_MATCH(LEX_R_UNDEFINED);
    return 0;
  } else if (lex->tk==LEX_STR) {
    JsVar *a = 0;
    if (JSP_SHOULD_EXECUTE)
      a = jslGetTokenValueAsVar(lex);
    JSP_ASSERT_MATCH(LEX_STR);
    return a;
#ifndef SAVE_ON_FLASH
  } else if (lex->tk==LEX_TEMPLATE_LITERAL) {
    return jspeTemplateLiteral();
#endif
  } else if (lex->tk==LEX_REGEX) {
    JsVar *a = 0;
#ifdef SAVE_ON_FLASH
    jsExceptionHere(JSET_SYNTAXERROR, "RegEx are not supported in this version of Espruino\n");
#else
    JsVar *regex = jslGetTokenValueAsVar(lex);
    size_t regexEnd = 0, regexLen = 0;
    JsvStringIterator it;
    jsvStringIteratorNew(&it, regex, 0);
    while (jsvStringIteratorHasChar(&it)) {
      regexLen++;
      if (jsvStringIteratorGetChar(&it)=='/')
        regexEnd = regexLen;
      jsvStringIteratorNext(&it);
    }
    jsvStringIteratorFree(&it);
    JsVar *flags = 0;
    if (regexEnd < regexLen)
      flags = jsvNewFromStringVar(regex, regexEnd, JSVAPPENDSTRINGVAR_MAXLENGTH);
    JsVar *regexSource = jsvNewFromStringVar(regex, 1, regexEnd-2);
    a = jswrap_regexp_constructor(regexSource, flags);
    jsvUnLock3(regex, flags, regexSource);
#endif
    JSP_ASSERT_MATCH(LEX_REGEX);
    return a;
  } else if (lex->tk=='{') {
    if (!jspCheckStackPosition()) return 0;
    return jspeFactorObject();
  } else if (lex->tk=='[') {
    if (!jspCheckStackPosition()) return 0;
    return jspeFactorArray();
  } else if (lex->tk==LEX_R_FUNCTION) {
    if (!jspCheckStackPosition()) return 0;
    JSP_ASSERT_MATCH(LEX_R_FUNCTION);
    return jspeFunctionDefinition(true);
#ifndef SAVE_ON_FLASH
  } else if (lex->tk==LEX_R_CLASS) {
    if (!jspCheckStackPosition()) return 0;
    JSP_ASSERT_MATCH(LEX_R_CLASS);
    return jspeClassDefinition(true);
  } else if (lex->tk==LEX_R_SUPER) {
    JSP_ASSERT_MATCH(LEX_R_SUPER);
    /* This is kind of nasty, since super appears to do
      three different things.

      * In the constructor it references the extended class's constructor
      * in a method it references the constructor's prototype.
      * in a static method it references the extended class's constructor (but this is different)
     */

    if (jsvIsObject(execInfo.thisVar)) {
      // 'this' is an object - must be calling a normal method
      JsVar *proto1 = jsvObjectGetChild(execInfo.thisVar, JSPARSE_INHERITS_VAR, 0); // if we're in a method, get __proto__ first
      JsVar *proto2 = jsvIsObject(proto1) ? jsvObjectGetChild(proto1, JSPARSE_INHERITS_VAR, 0) : 0; // still in method, get __proto__.__proto__
      jsvUnLock(proto1);
      if (!proto2) {
        jsExceptionHere(JSET_SYNTAXERROR, "Calling 'super' outside of class");
        return 0;
      }
      if (lex->tk=='(') return proto2; // eg. used in a constructor
      // But if we're doing something else - eg '.' or '[' then it needs to reference the prototype
      JsVar *proto3 = jsvIsFunction(proto2) ? jsvObjectGetChild(proto2, JSPARSE_PROTOTYPE_VAR, 0) : 0;
      jsvUnLock(proto2);
      return proto3;
    } else if (jsvIsFunction(execInfo.thisVar)) {
      // 'this' is a function - must be calling a static method
      JsVar *proto1 = jsvObjectGetChild(execInfo.thisVar, JSPARSE_PROTOTYPE_VAR, 0);
      JsVar *proto2 = jsvIsObject(proto1) ? jsvObjectGetChild(proto1, JSPARSE_INHERITS_VAR, 0) : 0;
      jsvUnLock(proto1);
      if (!proto2) {
        jsExceptionHere(JSET_SYNTAXERROR, "Calling 'super' outside of class");
        return 0;
      }
      return proto2;
    }
    jsExceptionHere(JSET_SYNTAXERROR, "Calling 'super' outside of class");
    return 0;
#endif
  } else if (lex->tk==LEX_R_THIS) {
    JSP_ASSERT_MATCH(LEX_R_THIS);
    return jsvLockAgain( execInfo.thisVar ? execInfo.thisVar : execInfo.root );
  } else if (lex->tk==LEX_R_DELETE) {
    if (!jspCheckStackPosition()) return 0;
    return jspeFactorDelete();
  } else if (lex->tk==LEX_R_TYPEOF) {
    if (!jspCheckStackPosition()) return 0;
    return jspeFactorTypeOf();
  } else if (lex->tk==LEX_R_VOID) {
    JSP_ASSERT_MATCH(LEX_R_VOID);
    jsvUnLock(jspeUnaryExpression());
    return 0;
  }
  JSP_MATCH(LEX_EOF);
  jsExceptionHere(JSET_SYNTAXERROR, "Unexpected end of Input\n");
  return 0;
}