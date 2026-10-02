/* One closed array registry shared by source admission and typed XLIR.
 * Macro consumers may select just names; physical types, item widths and
 * ownership remain verified from these same descriptors before emission. */
XL6_NATIVE_ARRAY_BRIDGE("xl6TextCharsV1",T,V,2,{S,I},C)
XL6_NATIVE_ARRAY_BRIDGE("xl6TextSplitV1",T,V,4,{S,S,I,I},S)
XL6_NATIVE_ARRAY_BRIDGE("xl6TextLinesV1",T,V,2,{S,I},S)
XL6_NATIVE_ARRAY_BRIDGE("xl6TextFieldsV1",T,V,2,{S,I},S)
XL6_NATIVE_ARRAY_BRIDGE("xl6TextJoinV1",T,S,4,{V,S,I,I},.parameterArrayItemAbis={S})
XL6_NATIVE_ARRAY_BRIDGE("xl6RegexSetCreateV1",T,V,6,{V,I,I,I,I,I},.parameterArrayItemAbis={S},.resultOwner="xl6RegexSetDestroyV1")
XL6_NATIVE_ARRAY_BRIDGE("xl6RegexSetHitsV1",T,V,4,{V,S,I,I},I,.parameterOwners={"xl6RegexSetDestroyV1"})
