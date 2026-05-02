int test_curve(int c) {
    spiro_cp spiro[16];
    int nextknot[17];
    double d[5];
    spiro_seg *segs = NULL;
    bezctx *bc;
    rs_check_vals *rsp;
    int i,j,done;

    /* Load sample data so that we can see if library is callable */
    load_test_curve(spiro,nextknot,c);

    d[0] = 1.; d[1] = d[1] = 0.;
#if defined(DO_CALL_TEST20)
    /* check if spiro values are reversed correctly on input path */
    printf("---\ntesting spiroreverse() using data=path%d[].\n",c);
    if ( (spiroreverse(spiro,cl[c])) ) {
	fprintf(stderr,"error with spiroreverse() using data=path%d[].\n",c);
	return -1;
    }
    /* just do a visual check to verify types and points look ok. */
    /* NOTE: spiro[] is NOT replaced if unable to reverse values. */
    for (i=0; i < cl[c]; i++) {
	printf("  reversed %d: ty=%c, x=%g, y=%g\n", i,spiro[i].ty,spiro[i].x,spiro[i].y);
    }
#else
#if defined(DO_CALL_TEST14) || defined(DO_CALL_TEST15) || defined(DO_CALL_TEST16) || defined(DO_CALL_TEST17) || defined(DO_CALL_TEST18) || defined(DO_CALL_TEST19)
#if defined(DO_CALL_TEST16) || defined(DO_CALL_TEST17) || defined(DO_CALL_TEST18) || defined(DO_CALL_TEST19)
    /* run_spiro0 with various paths to test a simple arc output. */
    d[0] = -1.;
    printf("---\ntesting run_spiro0(q) using data=path%d[].\n",c);
    if ( (segs=run_spiro0(spiro,d,SPIRO_ARC_MAYBE,cl[c]))==0 ) {
	fprintf(stderr,"error with run_spiro0(q) using data=path%d[].\n",c);
	return -1;
    }
#else
    /* Do run_spiro0 instead (these tests are far from -0.5..+0.5 */
    d[0] = -1.;
    printf("---\ntesting run_spiro0() using data=path%d[].\n",c);
    if ( (segs=run_spiro0(spiro,d,0,cl[c]))==0 ) {
	fprintf(stderr,"error with run_spiro0() using data=path%d[].\n",c);
	return -1;
    }
#endif
#else
    /* Check if run_spiro works okay (try backwards compatiblity) */
    printf("---\ntesting run_spiro() using data=path%d[].\n",c);
    if ( (segs=run_spiro(spiro,cl[c]))==0 ) {
	fprintf(stderr,"error with run_spiro() using data=path%d[].\n",c);
	return -1;
    }
#endif

    /* Load pointer to verification data to ensure it works right */
    if ( c==0 )	     rsp = verify_rs0;
    else if ( c==1 ) rsp = verify_rs1;
    else if ( c==2 ) rsp = verify_rs2;
    /* else if ( c==3 ) rsp = NULL; expecting failure to converge */
    else if ( c==4 ) rsp = verify_rs4;
    else if ( c==5 ) rsp = verify_rs5;
    else if ( c==6 ) rsp = verify_rs5; /* same angles used for #6 */
    else if ( c==7 ) rsp = verify_rs7; /* stop curve with ah code */
    else if ( c==8 ) rsp = verify_rs8;
    else if ( c==9 ) rsp = verify_rs9;
    else if ( c==10 ) rsp = verify_rs10; /* start curve using ah. */
    else if ( c==11 ) rsp = verify_rs11;
    else if ( c==12 ) rsp = verify_rs7; /* test #12 uses path7[]. */
    else if ( c==13 ) rsp = verify_rs13; /* almost same as path11 */
    else if ( c==14 ) rsp = verify_rs14; /* very large path9 copy */
    else if ( c==15 ) rsp = verify_rs15; /* sub-atomic path9 copy */
    else if ( c==16 ) rsp = verify_rs4; /* path4 arc curve output */
    else if ( c==17 ) rsp = verify_rs4; /* path4 arc curve closed */
    else if ( c==18 ) rsp = verify_rs2; /* trying many iterations */
    else	      rsp = verify_rs0; /* long list, arc output. */

    /* Quick visual check shows X,Y knots match with each pathN[] */
    for (i=j=0; i < cl[c]-1; i++,j++) {
	if ( spiro[i].ty == 'h' ) {
	    --j;
	    printf("curve %d ctrl %d t=%c x=%f y=%f (handle)\n", \
	      c,j,segs[i].ty,spiro[i].x,spiro[i].y);
	}
	printf("curve %d line %d t=%c x=%f y=%f bend=%f ch=%f th=%f ",c,j, \
	  segs[i].ty,segs[i].x,segs[i].y,segs[i].bend_th,segs[i].seg_ch,segs[i].seg_th);
	/* Let computer verify that run_spiro() data is PASS/FAIL */
	/* Tests including ah data more complicated to verify xy, */
	/* therefore, skip testing xy for call_tests shown below. */
	if ( (segs[i].ty != spiro[i].ty) ||
#if defined(DO_CALL_TEST14) || defined(DO_CALL_TEST15) || defined(DO_CALL_TEST16) || defined(DO_CALL_TEST17) || defined(DO_CALL_TEST18) || defined(DO_CALL_TEST19)
#if defined(DO_CALL_TEST14) || defined(DO_CALL_TEST16) || defined(DO_CALL_TEST17) || defined(DO_CALL_TEST18) || defined(DO_CALL_TEST19)
	     (fabs((segs[i].x * d[0] + d[1]) - spiro[i].x) > 1e5) ||
	     (fabs((segs[i].y * d[0] + d[2]) - spiro[i].y) > 1e5) ||
	     (fabs(segs[i].seg_ch * d[0] - rsp[i].ch) > 1e5) ||
#else
	     (fabs((segs[i].x * d[0] + d[1]) - spiro[i].x) > 1e-47) ||
	     (fabs((segs[i].y * d[0] + d[2]) - spiro[i].y) > 1e-47) ||
	     (fabs(segs[i].seg_ch * d[0] - rsp[i].ch) > 1e-47) ||
#endif
#else
#if !defined(DO_CALL_TEST6) && !defined(DO_CALL_TEST7) && !defined(DO_CALL_TEST8) && !defined(DO_CALL_TEST10) && !defined(DO_CALL_TEST11) && !defined(DO_CALL_TEST12) && !defined(DO_CALL_TEST13)
	     (fabs(segs[i].x - spiro[i].x) > 1e-5) ||
	     (fabs(segs[i].y - spiro[i].y) > 1e-5) ||
#endif
	     (fabs(segs[i].seg_ch - rsp[i].ch) > 1e-5) ||
#endif
	     (fabs(segs[i].bend_th - rsp[i].b) > 1e-5) ||
	     (fabs(segs[i].seg_th - rsp[i].th) > 1e-5) ) {
	  printf("FAIL\n");
	  fprintf(stderr,"Error found with run_spiro() data. Results are not the same.\n");
	    fprintf(stderr,"expected line %d t=%c x=%f y=%f bend=%f ch=%f th=%f\n", j, \
	      spiro[i].ty,spiro[i].x,spiro[i].y,rsp[i].b,rsp[i].ch,rsp[i].th);
	    free(segs);
	    return -2;
	} else
	    printf("PASS\n");
    }
    printf("curve %d ",c);
    if ( spiro[i].ty == '}' || spiro[i].ty == 'z' )
	printf("stop %d t=%c x=%f y=%f\n",j,segs[i].ty,segs[i].x,segs[i].y);
    else
	printf("line %d t=%c x=%f y=%f bend=%f ch=%f th=%f\n", j,segs[i].ty, \
	  segs[i].x,segs[i].y,segs[i].bend_th,segs[i].seg_ch,segs[i].seg_th);

    /* Quick visual check shows X,Y knots match with each pathN[] */
    printf("---\ntesting spiro_to_bpath() using data from run_spiro(data=path%d[],len=%d).\n",c,cl[c]);
    bc = new_bezctx_test();
#if defined(DO_CALL_TEST14) || defined(DO_CALL_TEST15) || defined(DO_CALL_TEST16) || defined(DO_CALL_TEST17) || defined(DO_CALL_TEST18) || defined(DO_CALL_TEST19)
#if defined(DO_CALL_TEST16) || defined(DO_CALL_TEST17) || defined(DO_CALL_TEST18) || defined(DO_CALL_TEST19)
    spiro_to_bpath0(spiro,segs,d,SPIRO_ARC_MAYBE,cl[c],bc);
#else
    spiro_to_bpath0(spiro,segs,d,0,cl[c],bc);
#endif
#else
    spiro_to_bpath(segs,cl[c],bc);
#endif
    free(segs);
#endif

#if !defined(DO_CALL_TEST20)
#if defined(DO_CALL_TEST14) || defined(DO_CALL_TEST15) || defined(DO_CALL_TEST16) || defined(DO_CALL_TEST17) || defined(DO_CALL_TEST18) || defined(DO_CALL_TEST19)
#if defined(DO_CALL_TEST14) || defined(DO_CALL_TEST15) || defined(DO_CALL_TEST16) || defined(DO_CALL_TEST17)
    /* Now verify we also have simple arc output too */
    printf("---\ntesting SpiroCPsToBezier2() using data=path%d[].\n",c);
    if ( SpiroCPsToBezier2(spiro,cl[c],SPIRO_ARC_MAYBE,co[c],bc)!=1 ) {
	fprintf(stderr,"error with SpiroCPsToBezier2() using data=path%d[].\n",c);
	return -7;
    }
#endif
#if defined(DO_CALL_TEST18) || defined(DO_CALL_TEST19)
    printf("---\ntesting TaggedSpiroCPsToBezier2() using data=path%d[].\n",c);
    if ( TaggedSpiroCPsToBezier2(spiro,SPIRO_ARC_MAYBE,bc)!=1 ) {
	fprintf(stderr,"error with TaggedSpiroCPsToBezier2() using data=path%d[].\n",c);
	return -8;
    }
#endif
#else
#if !defined(DO_CALL_TEST4) && !defined(DO_CALL_TEST6) && !defined(DO_CALL_TEST7) && !defined(DO_CALL_TEST8) && !defined(DO_CALL_TEST9) && !defined(DO_CALL_TEST10) && !defined(DO_CALL_TEST11)
    /* Check if TaggedSpiroCPsToBezier0() works okay */
    printf("---\ntesting TaggedSpiroCPsToBezier0() using data=path%d[].\n",c);
    if ( TaggedSpiroCPsToBezier0(spiro,bc)!=1 ) {
	fprintf(stderr,"error with TaggedSpiroCPsToBezier0() using data=path%d[].\n",c);
	return -3;
    }
#endif

#if !defined(DO_CALL_TEST12)
    /* Check if SpiroCPsToBezier0() works okay */
    printf("---\ntesting SpiroCPsToBezier0() using data=path%d[].\n",c);
    if ( SpiroCPsToBezier0(spiro,cl[c],co[c],bc)!=1 ) {
	fprintf(stderr,"error with SpiroCPsToBezier0() using data=path%d[].\n",c);
	return -4;
    }
#endif

#if !defined(DO_CALL_TEST4) && !defined(DO_CALL_TEST6) && !defined(DO_CALL_TEST7) && !defined(DO_CALL_TEST8) && !defined(DO_CALL_TEST9) && !defined(DO_CALL_TEST10) && !defined(DO_CALL_TEST11)
    /* Check if TaggedSpiroCPsToBezier1() works okay */
    printf("---\ntesting TaggedSpiroCPsToBezier1() using data=path%d[].\n",c);
    TaggedSpiroCPsToBezier1(spiro,bc,&done);
    if ( done!=1 ) {
	fprintf(stderr,"error with TaggedSpiroCPsToBezier1() using data=path%d[].\n",c);
	return -5;
    }
#endif

#if !defined(DO_CALL_TEST12)
    /* Check if SpiroCPsToBezier1() works okay */
    printf("---\ntesting SpiroCPsToBezier1() using data=path%d[].\n",c);
    SpiroCPsToBezier1(spiro,cl[c],co[c],bc,&done);
    if ( done!=1 ) {
	fprintf(stderr,"error with SpiroCPsToBezier1() using data=path%d[].\n",c);
	return -6;
    }
#endif
#endif

#else
    /* We already visually checked output for spiroreverse above. */
    /* Some reversed paths (above) will fail (like path20), so we */
    /* reverse the reversed spiro path so we can use current test */
    /* functions & values (so that this actually tests something) */
    if (c == 20 || c == 21) {
	/* Check if SpiroCPsToBezier2() works okay */
	printf("---\ntesting SpiroCPsToBezier2(reverse) using data=path%d[].\n",c);
	if ( SpiroCPsToBezier2(spiro,cl[c],SPIRO_REVERSE_SRC,co[c],bc)!=1 ) {
	    fprintf(stderr,"error with SpiroCPsToBezier2(reverse) using data=path%d[].\n",c);
	    return -9;
	}
    } else { /* c==22 || c==23 || c==24 */
	/* Check if TaggedSpiroCPsToBezier2() works okay */
	printf("---\ntesting TaggedSpiroCPsToBezier2(reverse) using data=path%d[].\n",c);
	if ( TaggedSpiroCPsToBezier2(spiro,SPIRO_REVERSE_SRC,bc)!=1 ) {
	    fprintf(stderr,"error with TaggedSpiroCPsToBezier2(reverse) using data=path%d[].\n",c);
	    return -10;
	}
    }
#endif

    free(bc);
    return 0;
}