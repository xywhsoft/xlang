static int xl6CgenTaskSpawnArgTempScopeNeeds(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_CgenDeclCallArgTemp* temps;
    int tempCount;
    int tempCapacity;
    int savedTempId;

    if ( ctx == NULL || expr == NULL ) {
        return 0;
    }

    temps = NULL;
    tempCount = 0;
    tempCapacity = 0;
    savedTempId = ctx->tempId;
    xl6CgenDeclCollectTaskSpawnArgTemps(ctx, expr, &temps, &tempCount, &tempCapacity);
    ctx->tempId = savedTempId;
    xl6CgenDeclFreeCallArgTemps(temps, tempCapacity);
    return tempCount > 0;
}
