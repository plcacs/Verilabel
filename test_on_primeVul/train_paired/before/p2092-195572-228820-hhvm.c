xmlNodePtr SimpleXMLElement_exportNode(const Object& sxe) {
  assert(sxe->instanceof(SimpleXMLElement_classof()));
  auto data = Native::data<SimpleXMLElement>(sxe.get());
  return php_sxe_get_first_node(data, data->nodep());
}