## -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

def build(bld):
    obj = bld.create_ns3_module('topology-read', ['network'])
    obj.source = [
       'model/topology-reader.cc',
       'model/inet-topology-reader.cc',
       'model/orbis-topology-reader.cc',
       'model/rocketfuel-topology-reader.cc',
       'helper/topology-reader-helper.cc',
       'imn_reader/imnHelper.cc',
       'imn_reader/imnNode.cc',
       'imn_reader/imnLink.cc',
       'imn_reader/xmlGenerator.cc',
        ]

    module_test = bld.create_ns3_module_test_library('topology-read')
    module_test.source = [
        'test/rocketfuel-topology-reader-test-suite.cc',
        ]

    headers = bld(features='ns3header')
    headers.module = 'topology-read'
    headers.source = [
       'model/topology-reader.h',
       'model/inet-topology-reader.h',
       'model/orbis-topology-reader.h',
       'model/rocketfuel-topology-reader.h',
       'helper/topology-reader-helper.h',
       'imn_reader/imnHelper.h',
       'imn_reader/imnNode.h',
       'imn_reader/imnLink.h',
       'imn_reader/xmlGenerator.h',
        ]

    if bld.env['ENABLE_EXAMPLES']:
        bld.recurse('examples')

    bld.ns3_python_bindings()
