bool SQClass::NewSlot(SQSharedState *ss,const SQObjectPtr &key,const SQObjectPtr &val,bool bstatic)
{
    SQObjectPtr temp;
    bool belongs_to_static_table = sq_type(val) == OT_CLOSURE || sq_type(val) == OT_NATIVECLOSURE || bstatic;
    if(_locked && !belongs_to_static_table)
        return false; //the class already has an instance so cannot be modified
    if(_members->Get(key,temp) && _isfield(temp)) //overrides the default value
    {
        _defaultvalues[_member_idx(temp)].val = val;
        return true;
    }
    if(belongs_to_static_table) {
        SQInteger mmidx;
        if((sq_type(val) == OT_CLOSURE || sq_type(val) == OT_NATIVECLOSURE) &&
            (mmidx = ss->GetMetaMethodIdxByName(key)) != -1) {
            _metamethods[mmidx] = val;
        }
        else {
            SQObjectPtr theval = val;
            if(_base && sq_type(val) == OT_CLOSURE) {
                theval = _closure(val)->Clone();
                _closure(theval)->_base = _base;
                __ObjAddRef(_base); //ref for the closure
            }
            if(sq_type(temp) == OT_NULL) {
                bool isconstructor;
                SQVM::IsEqual(ss->_constructoridx, key, isconstructor);
                if(isconstructor) {
                    _constructoridx = (SQInteger)_methods.size();
                }
                SQClassMember m;
                m.val = theval;
                _members->NewSlot(key,SQObjectPtr(_make_method_idx(_methods.size())));
                _methods.push_back(m);
            }
            else {
                _methods[_member_idx(temp)].val = theval;
            }
        }
        return true;
    }
    SQClassMember m;
    m.val = val;
    _members->NewSlot(key,SQObjectPtr(_make_field_idx(_defaultvalues.size())));
    _defaultvalues.push_back(m);
    return true;
}