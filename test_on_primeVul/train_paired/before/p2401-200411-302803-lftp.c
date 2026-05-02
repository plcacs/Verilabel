int   MirrorJob::Do()
{
   int	 res;
   int	 m=STALL;
   FileInfo *file;
   Job	 *j;

   switch(state)
   {
   case(INITIAL_STATE):
      remove_this_source_dir=(remove_source_dirs && source_dir.last_char()!='/');
      if(!strcmp(target_dir,".") || !strcmp(target_dir,"..") || (FlagSet(SCAN_ALL_FIRST) && parent_mirror))
	 create_target_dir=false;

      source_session->Chdir(source_dir);
      source_redirections=0;
      source_session->Roll();
      set_state(CHANGING_DIR_SOURCE);
      m=MOVED;
      /*fallthrough*/
   case(CHANGING_DIR_SOURCE):
      HandleChdir(source_session,source_redirections);
      if(state!=CHANGING_DIR_SOURCE)
	 return MOVED;
      if(source_session->IsOpen())
	 return m;

      source_dir.set(source_session->GetCwd().GetDirectory());

   pre_MAKE_TARGET_DIR:
   {
      if(!create_target_dir)
	 goto pre_CHANGING_DIR_TARGET;
      if(target_is_local)
      {
	 struct stat st;
	 if((FlagSet(RETR_SYMLINKS)?stat:lstat)(target_dir,&st)!=-1)
	 {
	    if(S_ISDIR(st.st_mode))
	    {
	       // try to enable write access
	       // only if not enabled as chmod can clear sgid flags on directories
	       if(!script_only && (st.st_mode!=(st.st_mode|0700)))
		  chmod(target_dir,st.st_mode|0700);
	       create_target_dir=false;
	       goto pre_CHANGING_DIR_TARGET;
	    }
	    else
	    {
	       Report(_("Removing old local file `%s'"),target_dir.get());
	       if(script)
	       {
		  ArgV args("rm");
		  args.Append(target_session->GetFileURL(target_dir));
		  xstring_ca cmd(args.CombineQuoted());
		  fprintf(script,"%s\n",cmd.get());
	       }
	       if(!script_only)
	       {
		  if(remove(target_dir)==-1)
		     eprintf("mirror: remove(%s): %s\n",target_dir.get(),strerror(errno));
	       }
	    }
	 }
      }

      if(FlagSet(DEPTH_FIRST))
	 goto pre_GETTING_LIST_INFO;

      if(target_relative_dir)
	 Report(_("Making directory `%s'"),target_relative_dir.get());
      bool mkdir_p=(parent_mirror==0 || parent_mirror->create_target_dir);
      if(script)
      {
	 ArgV args("mkdir");
	 if(mkdir_p)
	    args.Append("-p");
	 args.Append(target_session->GetFileURL(target_dir));
	 xstring_ca cmd(args.CombineQuoted());
	 fprintf(script,"%s\n",cmd.get());
	 if(script_only)
	    goto pre_CHANGING_DIR_TARGET;
      }
      target_session->Mkdir(target_dir,mkdir_p);
      set_state(MAKE_TARGET_DIR);
      m=MOVED;
   }
      /*fallthrough*/
   case(MAKE_TARGET_DIR):
      res=target_session->Done();
      if(res==FA::IN_PROGRESS)
	 return m;
      target_session->Close();
      create_target_dir=false;

   pre_CHANGING_DIR_TARGET:
      target_session->Chdir(target_dir);
      target_redirections=0;
      target_session->Roll();
      set_state(CHANGING_DIR_TARGET);
      m=MOVED;
      /*fallthrough*/
   case(CHANGING_DIR_TARGET):
      HandleChdir(target_session,target_redirections);
      if(state!=CHANGING_DIR_TARGET)
	 return MOVED;
      if(target_session->IsOpen())
	 return m;
      create_target_dir=false;

      target_dir.set(target_session->GetCwd().GetDirectory());

   pre_GETTING_LIST_INFO:
      set_state(GETTING_LIST_INFO);
      m=MOVED;
      if(!source_set)
	 HandleListInfoCreation(source_session,source_list_info,source_relative_dir);
      if(!target_set && !create_target_dir
      && (!FlagSet(DEPTH_FIRST) || FlagSet(ONLY_EXISTING))
      && !(FlagSet(TARGET_FLAT) && parent_mirror))
	 HandleListInfoCreation(target_session,target_list_info,target_relative_dir);
      if(state!=GETTING_LIST_INFO)
      {
	 source_list_info=0;
	 target_list_info=0;
      }
      return m;	  // give time to other tasks
   case(GETTING_LIST_INFO):
      HandleListInfo(source_list_info,source_set);
      HandleListInfo(target_list_info,target_set,&target_set_excluded);
      if(state!=GETTING_LIST_INFO)
	 return MOVED;
      if(source_list_info || target_list_info)
	 return m;

      MirrorFinished(); // leave room for transfers.

      if(FlagSet(DEPTH_FIRST) && source_set && !target_set)
      {
	 // transfer directories first
	 InitSets();
	 to_transfer->Unsort();
	 to_transfer->SubtractNotDirs();
	 goto pre_WAITING_FOR_TRANSFER;
      }

      // now we have both target and source file sets.
      if(parent_mirror)
	 stats.dirs++;

      if(FlagSet(SCAN_ALL_FIRST) && parent_mirror)
      {
	 source_set->PrependPath(source_relative_dir);
	 if(root_mirror->source_set_recursive)
	    root_mirror->source_set_recursive->Merge(source_set);
	 else
	    root_mirror->source_set_recursive=source_set.borrow();
	 if(target_set) {
	    target_set->PrependPath(target_relative_dir);
	    if(root_mirror->target_set_recursive)
	       root_mirror->target_set_recursive->Merge(target_set);
	    else
	       root_mirror->target_set_recursive=target_set.borrow();
	 }
	 if(target_set_excluded) {
	    target_set_excluded->PrependPath(target_relative_dir);
	    if(root_mirror->target_set_excluded)
	       root_mirror->target_set_excluded->Merge(target_set_excluded);
	    else
	       root_mirror->target_set_excluded=target_set_excluded.borrow();
	 }
	 root_mirror->stats.dirs++;
	 transfer_count++; // parent mirror will decrement it.
	 goto pre_DONE;
      }

      if(source_set_recursive) {
	 source_set->Merge(source_set_recursive);
	 source_set_recursive=0;
      }
      if(target_set_recursive) {
	 target_set->Merge(target_set_recursive);
	 target_set_recursive=0;
      }
      InitSets();

      to_transfer->CountBytes(&bytes_to_transfer);
      if(parent_mirror)
	 parent_mirror->AddBytesToTransfer(bytes_to_transfer);

      to_rm->Count(&stats.del_dirs,&stats.del_files,&stats.del_symlinks,&stats.del_files);
      to_rm->rewind();
      to_rm_mismatched->Count(&stats.del_dirs,&stats.del_files,&stats.del_symlinks,&stats.del_files);
      to_rm_mismatched->rewind();

      target_set->Merge(target_set_excluded);
      target_set_excluded=0;

      set_state(TARGET_REMOVE_OLD_FIRST);
      goto TARGET_REMOVE_OLD_FIRST_label;

   pre_TARGET_MKDIR:
      if(!to_mkdir)
	 goto pre_WAITING_FOR_TRANSFER;
      to_mkdir->rewind();
      set_state(TARGET_MKDIR);
      m=MOVED;
      /*fallthrough*/
   case(TARGET_MKDIR):
      while((j=FindDoneAwaitedJob())!=0)
      {
	 JobFinished(j);
	 m=MOVED;
      }
      if(max_error_count>0 && stats.error_count>=max_error_count)
	 goto pre_FINISHING;
      while(transfer_count<parallel && state==TARGET_MKDIR)
      {
	 file=to_mkdir->curr();
	 if(!file)
	    goto pre_WAITING_FOR_TRANSFER;
	 to_mkdir->next();
	 if(!file->TypeIs(file->DIRECTORY))
	    continue;
	 if(script)
	    fprintf(script,"mkdir %s\n",target_session->GetFileURL(file->name).get());
	 if(!script_only)
	 {
	    ArgV *a=new ArgV("mkdir");
	    a->Append(file->name);
	    mkdirJob *mkj=new mkdirJob(target_session->Clone(),a);
	    a->CombineTo(mkj->cmdline);
	    JobStarted(mkj);
	    m=MOVED;
	 }
      }
      break;

   pre_WAITING_FOR_TRANSFER:
      to_transfer->rewind();
      set_state(WAITING_FOR_TRANSFER);
      m=MOVED;
      /*fallthrough*/
   case(WAITING_FOR_TRANSFER):
      while((j=FindDoneAwaitedJob())!=0)
      {
	 TransferFinished(j);
	 m=MOVED;
      }
      if(max_error_count>0 && stats.error_count>=max_error_count)
	 goto pre_FINISHING;
      while(transfer_count<parallel && state==WAITING_FOR_TRANSFER)
      {
	 file=to_transfer->curr();
	 if(!file)
	 {
	    // go to the next step only when all transfers have finished
	    if(waiting_num>0)
	       break;
	    if(FlagSet(DEPTH_FIRST))
	    {
	       // we have been in the depth, don't go there again
	       SetFlags(DEPTH_FIRST,false);
	       SetFlags(NO_RECURSION,true);

	       // if we have not created any subdirs and there are only subdirs,
	       // then the directory would be empty - skip it.
	       if(FlagSet(NO_EMPTY_DIRS) && stats.dirs==0 && only_dirs)
		  goto pre_FINISHING_FIX_LOCAL;

	       MirrorStarted();
	       goto pre_MAKE_TARGET_DIR;
	    }
	    goto pre_TARGET_REMOVE_OLD;
	 }
	 HandleFile(file);
	 to_transfer->next();
	 m=MOVED;
      }
      break;

   pre_TARGET_REMOVE_OLD:
      if(FlagSet(REMOVE_FIRST))
	 goto pre_TARGET_CHMOD;
      set_state(TARGET_REMOVE_OLD);
      m=MOVED;
      /*fallthrough*/
   case(TARGET_REMOVE_OLD):
   case(TARGET_REMOVE_OLD_FIRST):
   TARGET_REMOVE_OLD_FIRST_label:
      while((j=FindDoneAwaitedJob())!=0)
      {
	 JobFinished(j);
	 m=MOVED;
      }
      if(max_error_count>0 && stats.error_count>=max_error_count)
	 goto pre_FINISHING;
      while(transfer_count<parallel && (state==TARGET_REMOVE_OLD || state==TARGET_REMOVE_OLD_FIRST))
      {
	 file=0;
	 if(!file && state==TARGET_REMOVE_OLD_FIRST)
	 {
	    file=to_rm_mismatched->curr();
	    to_rm_mismatched->next();
	 }
	 if(!file && (state==TARGET_REMOVE_OLD || FlagSet(REMOVE_FIRST)))
	 {
	    file=to_rm->curr();
	    to_rm->next();
	 }
	 if(!file)
	 {
	    if(waiting_num>0)
	       break;
	    if(state==TARGET_REMOVE_OLD)
	       goto pre_TARGET_CHMOD;
	    goto pre_TARGET_MKDIR;
	 }
	 if(!FlagSet(DELETE))
	 {
	    if(FlagSet(REPORT_NOT_DELETED))
	    {
	       const char *target_name_rel=dir_file(target_relative_dir,file->name);
	       if(file->TypeIs(file->DIRECTORY))
		  Report(_("Old directory `%s' is not removed"),target_name_rel);
	       else
		  Report(_("Old file `%s' is not removed"),target_name_rel);
	    }
	    continue;
	 }
	 if(script)
	 {
	    ArgV args("rm");
	    if(file->TypeIs(file->DIRECTORY))
	    {
	       if(recursion_mode==RECURSION_NEVER)
		  args.setarg(0,"rmdir");
	       else
		  args.Append("-r");
	    }
	    args.Append(target_session->GetFileURL(file->name));
	    xstring_ca cmd(args.CombineQuoted());
	    fprintf(script,"%s\n",cmd.get());
	 }
	 if(!script_only)
	 {
	    ArgV *args=new ArgV("rm");
	    args->Append(file->name);
	    args->seek(1);
	    rmJob *j=new rmJob(target_session->Clone(),args);
	    args->CombineTo(j->cmdline);
	    JobStarted(j);
	    if(file->TypeIs(file->DIRECTORY))
	    {
	       if(recursion_mode==RECURSION_NEVER)
	       {
		  args->setarg(0,"rmdir");
		  j->Rmdir();
	       }
	       else
		  j->Recurse();
	    }
	 }
	 const char *target_name_rel=dir_file(target_relative_dir,file->name);
	 if(file->TypeIs(file->DIRECTORY))
	    Report(_("Removing old directory `%s'"),target_name_rel);
	 else
	    Report(_("Removing old file `%s'"),target_name_rel);
      }
      break;

   pre_TARGET_CHMOD:
      if(FlagSet(NO_PERMS))
	 goto pre_FINISHING_FIX_LOCAL;

      to_transfer->rewind();
      if(FlagSet(TARGET_FLAT))
	 to_transfer->Sort(FileSet::BYNAME_FLAT);
      set_state(TARGET_CHMOD);
      m=MOVED;
      /*fallthrough*/
   case(TARGET_CHMOD):
      while((j=FindDoneAwaitedJob())!=0)
      {
	 JobFinished(j);
	 m=MOVED;
      }
      if(max_error_count>0 && stats.error_count>=max_error_count)
	 goto pre_FINISHING;
      while(transfer_count<parallel && state==TARGET_CHMOD)
      {
	 file=to_transfer->curr();
	 if(!file)
	    goto pre_FINISHING_FIX_LOCAL;
	 to_transfer->next();
	 if(file->TypeIs(file->SYMLINK))
	    continue;
	 if(!file->Has(file->MODE))
	    continue;
	 mode_t mode_mask=get_mode_mask();
	 mode_t def_mode=(file->TypeIs(file->DIRECTORY)?0775:0664)&~mode_mask;
	 if(target_is_local && file->mode==def_mode)
	 {
	    struct stat st;
	    if(!target_is_local || lstat(dir_file(target_dir,file->name),&st)==-1)
	       continue;
	    if((st.st_mode&07777)==(file->mode&~mode_mask))
	       continue;
	 }
	 FileInfo *target=target_set->FindByName(file->name);
	 if(target && target->filetype==file->DIRECTORY && file->filetype==file->DIRECTORY
	 && target->mode==(file->mode&~mode_mask) && (target->mode&0200))
	    continue;
	 if(script)
	 {
	    ArgV args("chmod");
	    args.Append(xstring::format("%03lo",(unsigned long)(file->mode&~mode_mask)));
	    args.Append(target_session->GetFileURL(file->name));
	    xstring_ca cmd(args.CombineQuoted());
	    fprintf(script,"%s\n",cmd.get());
	 }
	 if(!script_only)
	 {
	    ArgV *a=new ArgV("chmod");
	    a->Append(file->name);
	    a->seek(1);
	    ChmodJob *cj=new ChmodJob(target_session->Clone(),
				 file->mode&~mode_mask,a);
	    a->CombineTo(cj->cmdline);
	    if(!verbose_report)
	       cj->BeQuiet(); // chmod is not supported on all servers; be quiet.
	    JobStarted(cj);
	    m=MOVED;
	 }
      }
      break;

   pre_FINISHING_FIX_LOCAL:
      if(target_is_local && !script_only)     // FIXME
      {
	 const bool flat=FlagSet(TARGET_FLAT);
	 to_transfer->Sort(FileSet::BYNAME_FLAT);
	 to_transfer->LocalUtime(target_dir,/*only_dirs=*/true,flat);
	 if(FlagSet(ALLOW_CHOWN))
	    to_transfer->LocalChown(target_dir,flat);
	 if(!FlagSet(NO_PERMS) && same)
	    same->LocalChmod(target_dir,get_mode_mask(),flat);
	 if(FlagSet(ALLOW_CHOWN) && same)
	    same->LocalChown(target_dir,flat);
      }
      if(remove_source_files && (same || to_rm_src))
	 goto pre_SOURCE_REMOVING_SAME;
   pre_FINISHING:
      set_state(FINISHING);
      m=MOVED;
      /*fallthrough*/
   case(FINISHING):
      while((j=FindDoneAwaitedJob())!=0)
      {
	 JobFinished(j);
	 m=MOVED;
      }
      if(waiting_num>0)
	 break;

      // all jobs finished.
      if(remove_this_source_dir) {
	 // remove source directory once.
	 remove_this_source_dir=false;
	 if(script)
	 {
	    ArgV args("rmdir");
	    args.Append(source_session->GetFileURL(source_dir));
	    xstring_ca cmd(args.CombineQuoted());
	    fprintf(script,"%s\n",cmd.get());
	 }
	 if(!script_only)
	 {
	    ArgV *args=new ArgV("rmdir");
	    args->Append(source_dir);
	    args->seek(1);
	    rmJob *j=new rmJob(source_session->Clone(),args);
	    args->CombineTo(j->cmdline);
	    j->Rmdir();
	    JobStarted(j);
	 }
	 if(source_relative_dir)
	    Report(_("Removing source directory `%s'"),source_relative_dir.get());
	 m=MOVED;
	 break;
      }

      // all jobs finished and src dir removed, if needed.

      transfer_count++; // parent mirror will decrement it.
      if(parent_mirror)
	 parent_mirror->stats.Add(stats);
      else
      {
	 if(stats.HaveSomethingDone(flags) && on_change)
	 {
	    CmdExec *exec=new CmdExec(source_session->Clone(),0);
	    AddWaiting(exec);
	    exec->FeedCmd(on_change);
	    exec->FeedCmd("\n");
	    set_state(LAST_EXEC);
	    break;
	 }
      }
      goto pre_DONE;

   pre_SOURCE_REMOVING_SAME:
      if(!same)
	 same=to_rm_src.borrow();
      else if(to_rm_src)
	 same->Merge(to_rm_src);
      same->rewind();
      set_state(SOURCE_REMOVING_SAME);
      m=MOVED;
      /*fallthrough*/
   case(SOURCE_REMOVING_SAME):
      while((j=FindDoneAwaitedJob())!=0)
      {
	 JobFinished(j);
	 m=MOVED;
      }
      if(max_error_count>0 && stats.error_count>=max_error_count)
	 goto pre_FINISHING;
      while(transfer_count<parallel && state==SOURCE_REMOVING_SAME)
      {
	 file=same->curr();
	 same->next();
	 if(!file)
	    goto pre_FINISHING;
	 if(file->TypeIs(file->DIRECTORY))
	    continue;
	 if(script)
	 {
	    ArgV args("rm");
	    args.Append(source_session->GetFileURL(file->name));
	    xstring_ca cmd(args.CombineQuoted());
	    fprintf(script,"%s\n",cmd.get());
	 }
	 if(!script_only)
	 {
	    ArgV *args=new ArgV("rm");
	    args->Append(file->name);
	    args->seek(1);
	    rmJob *j=new rmJob(source_session->Clone(),args);
	    args->CombineTo(j->cmdline);
	    JobStarted(j);
	 }
	 const char *source_name_rel=dir_file(source_relative_dir,file->name);
	 Report(_("Removing source file `%s'"),source_name_rel);
      }
      break;

   case(LAST_EXEC):
      while((j=FindDoneAwaitedJob())!=0)
      {
	 RemoveWaiting(j);
	 Delete(j);
	 m=MOVED;
      }
      if(waiting_num>0)
	 break;
   pre_DONE:
      set_state(DONE);
      m=MOVED;
      bytes_transferred=0;
      if(!parent_mirror && FlagSet(LOOP) && stats.HaveSomethingDone(flags) && !stats.error_count)
      {
	 PrintStatus(0,"");
	 printf(_("Retrying mirror...\n"));
	 stats.Reset();
	 source_set=0;
	 target_set=0;
	 goto pre_GETTING_LIST_INFO;
      }
      /*fallthrough*/
   case(DONE):
      break;
   }
   // give direct parent priority over grand-parents.
   if(transfer_count<parallel && parent_mirror)
      m|=parent_mirror->Roll();
   return m;
}