xmlNodePtr SimpleXMLElement_exportNode(const Object& sxe) {
  if (!sxe->instanceof(SimpleXMLElement_classof())) return nullptr;
  auto data = Native::data<SimpleXMLElement>(sxe.get());
  return php_sxe_get_first_node(data, data->nodep());
}