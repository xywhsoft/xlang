int xlsRawBodyPreprocessInputs(xstrbuf* out,const XL6_SourceReport* report)
{
 if(!out || out->Size || !report || !report->complete || report->errorPath || report->error[0] ||
    report->count>XL6_SOURCE_REPORT_FILES || (report->count && !report->items) || report->probeCount>65536 ||
    (report->probeCount && !report->probes) || !xlsRawBodyEnvironmentCopy(out))return 0;
 if(!xlsSemKeyNumber(out,report->count))return 0;
 for(size_t i=0;i<report->count;++i){const XL6_SourceReportItem* item=&report->items[i];
  if(!item->path || (item->kind!=XL6_SOURCE_REPORT_SOURCE && item->kind!=XL6_SOURCE_REPORT_MANIFEST) ||
     !xlsSemKeyNumber(out,(uint64_t)item->kind) || !xlsSemKeyText(out,item->path) || !xlsSemKeyText(out,item->origin))return 0;
  if(item->kind==XL6_SOURCE_REPORT_SOURCE){if(!xlsRawBodyEnvironmentHasPath(item->path))return 0;}
  else if((item->size && !item->text) || !xlsSemKeyNumber(out,item->text!=NULL) || !xlsSemKeyNumber(out,item->size) ||
          !xlsSemKeyNumber(out,item->rawSize) || !xlsSemKeyBytes(out,item->textSha256,sizeof(item->textSha256)) ||
          !xlsSemKeyBytes(out,item->rawSha256,sizeof(item->rawSha256)) ||
          (item->size && !xlsSemKeyBytes(out,item->text,item->size)))return 0;
 }
 if(!xlsSemKeyNumber(out,report->probeCount))return 0;
 for(size_t i=0;i<report->probeCount;++i)if(!report->probes[i].path || !xlsSemKeyText(out,report->probes[i].path) ||
     !xlsSemKeyNumber(out,(uint64_t)report->probes[i].exists))return 0;
 return xlsSemKeyNumber(out,(uint64_t)report->packagePathObserved) && xlsSemKeyText(out,report->packagePath) && out->Capacity<=XLS_SEM_KEY_BYTES;
}
