# Copyright Epic Games, Inc. All Rights Reserved.
import unreal 

# This example is designed to show you how to create a Movie Graph Configuration asset,
# and add several settings nodes, configure settings on them, and then wire the nodes
# together into a graph. This is finally saved as a package in the Content Browser,
# so you can pick it in the Movie Render Queue UI (after converting a job to use the graph)
# 
# USAGE:
#   - Requires the "Python Editor Script Plugin" to be enabled in your project.
#
#   Open the Python interactive console and use:
#       import MovieGraphCreateConfigExample
#       MovieGraphCreateConfigExample.CreateBasicConfig()
#       OR
#       MovieGraphCreateConfigExample.CreateIntermediateConfig()
#       OR
#       MovieGraphCreateConfigExample.CreateAdvancedConfig()
#
# NOTES:
#
#    - Currently the "Select" nodes are not exposed to the Python API due to their complexity.
#      They internally use dynamic data types which makes it difficult to expose to Python.

def ConvertJobToGraph_Internal(graph_config):
    subsystem = unreal.get_editor_subsystem(unreal.MoviePipelineQueueSubsystem)
    pipelineQueue = subsystem.get_queue()
    if(len(pipelineQueue.get_jobs()) == 0):
        unreal.log_error("Open the Window > Movie Render Queue and add at least one job to use this example.")
        return None
    
    job = pipelineQueue.get_jobs()[0]
    
    # This converts the job to use the Graph Configuration asset for rendering,
    # instead of the standard MoviePipelinePrimaryConfig. Note that unlike the old
    # system, the new one doesn't currently support "internal" configs (ie: the
    # old system had SetPresetOrigin and SetConfiguration, using SetConfiguration
    # would copy the asset into the internal configuration, while SetPresetOrigin
    # was used to link to assets on disk. The new system only ever supports assets
    # on disk, and is similar to calling SetPresetOrigin in the old system).
    job.set_graph_preset(graph_config)
    
    return job

def CreateBasicConfig_Internal(config_asset_name):
    asset_directory = "/Game/MyTests/"
    
    # Clean up in case the package was already created (since this is sample code). Normally you'd want to do something like unreal.editor_asset_library.load_asset("/Game/MyTests/BasicConfig")
    # if you wanted to edit an existing asset. Because this example is about creating assets, we're going to just delete it if it exists.
    if unreal.EditorAssetLibrary.does_asset_exist(asset_directory + config_asset_name):
        unreal.EditorAssetLibrary.delete_asset(asset_directory + config_asset_name)
    
    # Create a new UMovieGraphConfig asset in a package (inside the /Content/MyTests/ folder). If you used "unreal.MovieGraphConfigFactory()" instead of "None"
    # it would copy the template specified in the Project Settings. This sample uses a totally blank asset for consistency since we don't know what users will have
    # set up in their project settings.
    graph_config = unreal.AssetToolsHelpers.get_asset_tools().create_asset(config_asset_name, asset_directory, unreal.MovieGraphConfig, None)
    
    # We're going to add the basic nodes to the graph that are required for simple renders - a render pass, an output format (png), a configured output directory, etc.
    output_setting_node = graph_config.create_node_by_class(unreal.MovieGraphGlobalOutputSettingNode)
    
    # For each setting you want to actually override with a given node, you need to set the override_ flag to True.
    output_setting_node.override_output_resolution = True
    output_setting_node.output_resolution = unreal.MovieGraphLibrary.named_resolution_from_profile("720p (HD)")
    
    # Create the png setting node
    png_setting_node = graph_config.create_node_by_class(unreal.MovieGraphImageSequenceOutputNode_PNG)
    
    # Find the output node (which comes with every graph), only the output node is considered during graph traversal,
    # so if nothing is connected to the output node, no nodes will be traversed.
    output_node = graph_config.get_output_node()
    
    # Connect the output settings node to our PNG node. A settings node that only uses default pins
    # (which are shown unlabeled in the editor UI) uses an empty pin name for the pins.
    graph_config.add_labeled_edge(output_setting_node, "", png_setting_node, "")
    
    # Now connect the png node to our output node
    graph_config.add_labeled_edge(png_setting_node, "", output_node, "Globals")
    
    # Now we can create a render layer. The globals tab could technically be used here instead, but for the sake of a more comprehensive example, we 
    # show the slightly more complicated setup since it better teaches a real graph.
    output_new_branch = graph_config.add_output()
    output_new_branch.set_member_name("main_branch")
    
    # Now we're going to create a few more nodes. The Deferred Renderer node (so this branch knows what type of render to produce),
    # and a Render Layer node (so that the system knows to consider this branch for rendering)
    deferred_renderer_node = graph_config.create_node_by_class(unreal.MovieGraphDeferredRenderPassNode)
    render_layer_node = graph_config.create_node_by_class(unreal.MovieGraphRenderLayerNode)
    
    # If you wanted to change the name that shows up in the {render layer} tokens, we use the name found in the render layer node, not the branch name.
    render_layer_node.override_layer_name = True
    render_layer_node.layer_name = "main_layer"
    
    # Connect the deferred renderer node to the layer name node
    graph_config.add_labeled_edge(deferred_renderer_node, "", render_layer_node, "")
    
    # Now connect the layer name node to the branch we created earlier so it actually gets considered during evaluation.
    graph_config.add_labeled_edge(render_layer_node, "", output_node, "main_branch")
    
    # If you want to use this graph as a sub-graph in another graph, you need to connect
    # the pins through to the Inputs node, so we'll do that here.
    input_new_branch = graph_config.add_input()
    input_new_branch.set_member_name("main_branch")
    graph_config.add_labeled_edge(graph_config.get_input_node(), "Globals", output_setting_node, "")
    graph_config.add_labeled_edge(graph_config.get_input_node(), "main_branch", deferred_renderer_node, "")
    
    # Now save the asset to disk.
    unreal.EditorAssetLibrary.save_asset(asset_directory + config_asset_name)
    return graph_config
    
def ApplyIntermediateConfigChanges_Internal(graph_config):
    # This example is going to show how to create "override" nodes, which are just second copies of
    # nodes that are further downstream in the example with their override fields also set. Then,
    # we will expose a specific property on that node and drive it with a graph variable, but also
    # showcase editing the job itself to drive that value.
    
    # Graph networks are too complex to simplify traversal, so you have to actually traverse each node
    # and consider what to do at each node. To do this, you get a Node, and then you get either it's Input
    # or Output pins, and then you can examine the edges connected to that pin, which give you the other end's pin,
    # and thus the next node in the chain.
    globals_pin_on_output = graph_config.get_output_node().get_input_pin("Globals")
    nodes_connected_to_globals_pin = globals_pin_on_output.get_connected_nodes()
    
    # Disconnect the first node from the Output's Globals pin. There should only be one node connected
    # in this example.
    first_node = nodes_connected_to_globals_pin[0]
    graph_config.remove_labeled_edge(first_node, "", graph_config.get_output_node(), "Globals")
   
    # Create a new node and connect it to the pins on either side...
    new_output_node = graph_config.create_node_by_class(unreal.MovieGraphGlobalOutputSettingNode)
    graph_config.add_labeled_edge(first_node, "", new_output_node, "")
    graph_config.add_labeled_edge(new_output_node, "", graph_config.get_output_node(), "Globals")
    
    # Specify that we want this new output node to override resolution
    new_output_node.override_output_resolution = True
    
    # Now, instead of hard-coding the resolution we want this override node to use, we want to expose the output
    # resolution pin into the graph so that we can connect things to it. Note that this uses the non-pythonified
    # variable name. Creating variables for use in graphs is a bit tricky right now as the underlying system can't
    # be easily exposed to Python. So we use a bit of a roundabout API - you start with a exposed property/pin,
    # and then let the internal system create a variable for you that has the correct setup to be connected to
    # that pin.
    
    # This exposes the OutputResolution property as a pin on the node in the graph. Note that you still need to check
    # bOverrideOutputResolution = True for the underlying system to decide to use that value.
    new_output_node.toggle_promote_property_to_pin("OutputResolution")
    
    # Request a new variable to be created of the right type to connect it to the OutputResolution node.
    # It will try to use the third argument (CustomOutputRes) as the name, but will ensure variable names are
    # unique so the returned name may not match (if a variable already existed).
    new_variable = graph_config.add_variable("CustomOutputRes")
    new_variable.set_value_type(unreal.MovieGraphValueType.STRUCT, unreal.MovieGraphNamedResolution.static_struct())
    
    # Now we can set a value for this variable. The node itself has been configured for 720p so we'll choose 
    # a different default. We use export_text() to turn the FMovieGraphNamedResolution into a string representation, and then
    # set the variable via set_value_serialized_string due to not allowing typed structs in Python. There exists
    # other set_value_<foo> functions for POD types.
    new_variable.set_value_serialized_string(unreal.MovieGraphLibrary.named_resolution_from_size(640, 480).export_text())
    
    # We have a node with the "OutputResolution" pin exposed, and we have a variable we want to drive it with. We need to
    # create another node on the graph and wire them together.
    new_variable_node = graph_config.create_node_by_class(unreal.MovieGraphVariableNode)
    
    # Associate this new variable node with the variable it should read from.
    new_variable_node.set_variable(new_variable)
    
    # Finally, wire the two nodes together. We use get_member_name() here instead of "CustomOutputRes", because we can't guarantee
    # that the variable was created with the suggested name earlier (ie: if there was a conflicting variable name already).
    graph_config.add_labeled_edge(new_variable_node, new_variable.get_member_name(), new_output_node, "OutputResolution")
    
    # To showcase the next thing, we're going to take a job from the Queue UI, and convert it to use a Graph Config,
    # and assign it to use our graph config. This will now create a copy of the variables from the graph config job,
    # which you can then set values on (such as an artist deciding to override it on the job and using a new value).
    job = ConvertJobToGraph_Internal(graph_config)
    if(job):
        # You can optionally override the values on a job, in case artists want to make top-level edits to the config
        # without actually editing the config assets. Like with individual nodes, you have to flag that you want to override
        # the value (without it, it will just use the default specified in the graph above, 640x480.)
        job_override_variables = job.get_or_create_variable_overrides(graph_config);
        job_override_variables.set_variable_assignment_enable_state(new_variable, True)
        job_override_variables.set_value_serialized_string(new_variable, unreal.MovieGraphLibrary.named_resolution_from_profile("1080p (FHD)").export_text())
    
def DeleteIntermediateConfigChanges_Internal(graph_config):
    # This function undoes the changes done by ApplyIntermediateConfigChanges_Internal. You generally wouldn't
    # write a script like this, but we want the examples to showcase how to delete variables, nodes, etc. as well
    # instead of just constructing graphs.
    all_graph_variables = graph_config.get_variables()
    
    # This just assumes the name was created as designed, which it should in our example case (the only reason it would have
    # a different name than intended is if there was a conflicting variable name when originally created.)
    for variable in all_graph_variables:
        if variable.get_member_name() == "CustomOutputRes": # Make sure to use get_member_name here and not get_name (which gets the internal object name)
            graph_config.delete_member(variable)
            break
    
    # Deleting the member variable also deleted any nodes that use the variable, so no need to clean that up. But we'll remove
    # the Output Node that we had added with the OutputResolution pin exposed.
    globals_pin_on_output = graph_config.get_output_node().get_input_pin("Globals")
    nodes_connected_to_globals_pin = globals_pin_on_output.get_connected_nodes()
 
    # The API only returns a list of nodes directly connected to the pin (not the whole hierarchy), so we need to 
    # run this query again on the node we're about to delete to figure out what was connected to it.
    nodes_connected_to_output_node = nodes_connected_to_globals_pin[0].get_input_pin("").get_connected_nodes()
    
    # Delete the first node (which should be the Output Node), which will also delete the connected edges.
    graph_config.remove_node(nodes_connected_to_globals_pin[0])
    
    # We now need to link the second node in the chain (a .png sequence in this example) back to the Globals pin on the outputs,
    # to keep graph connectivity.
    graph_config.add_labeled_edge(nodes_connected_to_output_node[0], "", graph_config.get_output_node(), "Globals")
            
    
def ApplyAdvancedConfigChanges_Internal(graph_config):
    # We're going to create a second render pass which is gated by a branch node that can be controlled
    # from the top level job (whether or not to include the given render pass).
    output_pt_branch = graph_config.add_output()
    output_pt_branch.set_member_name("path_tracer")
    
    # Create a branch node, then create a variable in the graph to drive it.
    branch_node = graph_config.create_node_by_class(unreal.MovieGraphBranchNode)
    graph_config.add_labeled_edge(branch_node, "", graph_config.get_output_node(), "path_tracer")
    
    use_pt_variable = graph_config.add_variable("AddPathTracerPass")
    use_pt_variable.set_value_type(unreal.MovieGraphValueType.BOOL)
    use_pt_node = graph_config.create_node_by_class(unreal.MovieGraphVariableNode)
    use_pt_node.set_variable(use_pt_variable)
    
    # Set the default value of the variable to True
    use_pt_variable.set_value_bool(True)
    
    # Connect the variable node to the branch node.
    graph_config.add_labeled_edge(use_pt_node, "AddPathTracerPass", branch_node, "Condition")
    
    # Create a matching inputs for pass-through
    input_with_pt = graph_config.add_input()
    input_with_pt.set_member_name("with_pt")
    
    input_no_pt = graph_config.add_input()
    input_no_pt.set_member_name("no_pt")
    
    pt_renderer_node = graph_config.create_node_by_class(unreal.MovieGraphPathTracedRenderPassNode)
    render_layer_node = graph_config.create_node_by_class(unreal.MovieGraphRenderLayerNode)
    render_layer_node.layer_name = "PathTracer"
    
    graph_config.add_labeled_edge(graph_config.get_input_node(), "with_pt", pt_renderer_node, "")
    graph_config.add_labeled_edge(pt_renderer_node, "", render_layer_node, "")
    
    # Wire both of these to the branch node
    graph_config.add_labeled_edge(render_layer_node, "", branch_node, "True")
    graph_config.add_labeled_edge(graph_config.get_input_node(), "no_pt", branch_node, "False")    
    
def CreateBasicConfig():    
    # The basic config example just makes a simple single-layer minimal example.
    CreateBasicConfig_Internal("BasicConfig")
    
def CreateIntermediateConfig():
    # The intermediate config creates a basic config, then takes some of the node properties and exposes them
    # as pins, and then drives those pins from user variables.
    basic_config = CreateBasicConfig_Internal("IntermediateConfig")
    ApplyIntermediateConfigChanges_Internal(basic_config)
    unreal.EditorAssetLibrary.save_loaded_asset(basic_config)
    
def CreateAdvancedConfig():
    # The advance config creates a basic config, applies the intermediate config changes, then un-applies them simply for the sake
    # of showing how to remove things from graphs as well. 
    basic_config = CreateBasicConfig_Internal("AdvanceConfig")
    ApplyIntermediateConfigChanges_Internal(basic_config)
    DeleteIntermediateConfigChanges_Internal(basic_config)
    
    # Now apply the advance changes to the config, ie: branching.
    ApplyAdvancedConfigChanges_Internal(basic_config)
    unreal.EditorAssetLibrary.save_loaded_asset(basic_config)