# Copyright Epic Games, Inc. All Rights Reserved.
#
#
# Helper functions for the Movie Render Graph, each function is a static method
# which helps testing individual functions in isolation. This module is used
# in the MovieGraphEditorExample python file 

import unreal

@staticmethod
def on_queue_finished_callback(executor: unreal.MoviePipelineExecutorBase, success: bool):
    """Is called after the executor has finished rendering all jobs

    Args:
        success (bool): True if all jobs completed successfully, false if a job 
                        encountered an error (such as invalid output directory)
                        or user cancelled a job (by hitting escape)
        executor (unreal.MoviePipelineExecutorBase): The executor that run this queue
    """
    unreal.log("on_queue_finished_callback Render completed. Success: " + str(success))


@staticmethod
def on_job_started_callback(job: unreal.MoviePipelineExecutorJob):
    """
    Called before the pipeline is initialized.
    
    Args:
        job (unreal.MoviePipelineExecutorJob): The job that will start as soon as the pipeline initializes.
    """

    unreal.log("on_job_started_callback() - Job Started")
    
    # Do anything you need to do here before the job starts. Here, we'll resolve the output directory that rendered media
    # is being saved into.
    
    graph = job.get_graph_preset()
    
    # There are multiple ways you can get the Global Output Settings node. If only one Global Output Settings node is in use, you
    # can fetch it directly from the graph preset. This is the easiest approach.
    output_settings_node = graph.get_node_for_branch(unreal.MovieGraphGlobalOutputSettingNode, "Globals")
    
    # In more complicated scenarios, you'll need to flatten/evaluate the graph first. This needs to be done if there
    # are multiple Global Output Settings nodes in use (eg, from subgraphs). A traversal context needs to be provided
    # to the evaluation process (so things like job variable overrides can be resolved correctly). Depending on what you're doing,
    # you may need to provide additional information in the traversal context, but generally this is enough.
    traversal_context = unreal.MovieGraphTraversalContext()
    traversal_context.shot_index = 0
    traversal_context.shot_count = 1
    traversal_context.job = job
    traversal_context.shot = job.shot_info[0]
    traversal_context.root_graph = graph
    
    # Evaluate the graph.
    flattened_graph, out_error = graph.create_flattened_graph(traversal_context)
    if flattened_graph:
        # Get the Global Output Settings node. Note that we're getting the CDO here (ie, the default) if there isn't actually a
        # Global Output Settings node in the Globals branch.
        branch_name = "Globals"
        include_cdos = True
        exact_match = True
        output_settings_node = flattened_graph.get_setting_for_branch(unreal.MovieGraphGlobalOutputSettingNode, branch_name, include_cdos, exact_match)
        
        # Resolving the output directory requires many parameters to be filled out. Depending on your use case, you may need more
        # parameters specified, or fewer.
        resolve_params = unreal.MovieGraphFilenameResolveParams()
        resolve_params.job = job
        resolve_params.shot = job.shot_info[0]
        resolve_params.evaluated_config = flattened_graph
        resolve_params.version = 1
        resolve_params.render_data_identifier.root_branch_name = "Globals"
        
        # Now resolve the output directory. Note that this will also provide a dictionary of the resolved format arguments (final_format_args). The
        # format arguments are the {tokens} in the directory path. This method is intended to be used to resolve a full file path and not a directory,
        # so it will contain a ".{ext}" at the end of the resolved path (which can just be stripped off for this example).
        resolved_path, final_format_args = unreal.MovieGraphLibrary.resolve_filename_format_arguments(output_settings_node.output_directory.path, resolve_params)
        resolved_path = resolved_path.replace(".{ext}", "")
        
        unreal.log("Resolved output directory: %s" % resolved_path)
    else:
        unreal.log("Unable to flatten graph. Error: %s" % out_error)

@staticmethod
def set_global_output_settings_node(job: unreal.MoviePipelineExecutorJob):
    '''
    This example demonstrates how to make a modification to the Global Output Settings
    node to edit the default values. If you are interested in just overriding some exposed
    values, check the "set_variable_overrides" which is a more appropriate workflow
    for per-job overrides.

    Note: This is modifying the actual shared graph asset and dirtying it.
    '''

    # Get the Graph Asset from the Job that we want to search for the Output Settings Node
    graph = job.get_graph_preset()

    # Get the Globals Output Node
    globals_pin_on_output = graph.get_output_node().get_input_pin("Globals")

    # Assume that the Output Settings Node is connected to the Globals Pin
    output_settings_node = globals_pin_on_output.get_connected_nodes()[0]
    
    if not isinstance(output_settings_node, unreal.MovieGraphGlobalOutputSettingNode):
        unreal.log("This example expected that the Global Output Settings node is plugged into the Globals Node")
        return

    output_settings_node.set_editor_property("override_output_resolution", True)
    # Override output resolution
    output_settings_node.set_editor_property("output_resolution", 
                unreal.MovieGraphLibrary.named_resolution_from_profile("720p (HD)"))


@staticmethod
def set_job_parameters(job: unreal.MoviePipelineExecutorJob):
    """This function showcases how job Parameters can be set or modified. By
    using the set_editor_property method, we ensure that the changes mark the Queue 
    as dirty

    Args:
        job (unreal.MoviePipelineExecutorJob): the Pipeline Job to be modified
    """
    job.set_editor_property("sequence", unreal.SoftObjectPath('/Game/Levels/shots/shot0010/shot0010.shot0010'))
    job.set_editor_property("map", unreal.SoftObjectPath('/Game/Levels/Main_LVL.Main_LVL'))

    job.set_editor_property("job_name", "shot0010")
    job.set_editor_property("author", "Automated.User")
    job.set_editor_property("comment", "This comment was created through Python")


@staticmethod
def set_variable_overrides(job: unreal.MoviePipelineExecutorJob):
    """Finds the variable override 'CustomOutputRes' and modifies it.

    Args:
        job (unreal.MoviePipelineExecutorJob): the Pipeline Job which we will 
                                        use to find the graph preset to modify
    """
    graph = job.get_graph_preset()
    variables = graph.get_variables()

    if not variables:
        print("No variables are exposed on this graph, expose 'CustomOutputRes' to test this example")

    # Find the variable to modify.
    custom_output_res_variable = graph.get_variable_by_name("CustomOutputRes")
    if not custom_output_res_variable:
        print("Could not find CustomOutputRes variable")
        return

    # Note: Alternatively, you can iterate the graph's variables to find the variable(s) that need to be modified.
    for variable in variables:
        # Get the variable's name with get_member_name(), NOT get_name()
        print("Found variable with name: %s" % variable.get_member_name())

    # When a variable's value is changed on a job, it's called a "variable assignment" or "variable override"; these
    # override the variable values coming from the graph. On the job, these overrides are grouped together by graph. If a
    # subgraph's variable(s) need to be overridden, those variable overrides need to be fetched separately (they will
    # not be included in the parent's variable overrides object).
    variable_overrides = job.get_or_create_variable_overrides(graph)

    # Set the variable override's value.
    #
    # This is done via set_*() methods on the overrides object; the exact set_*() method you call depends on the data type
    # of the variable: set_value_bool(), set_value_string(), set_value_int32(), etc. Look up the help docs to get the complete
    # set of methods. Run this in the UE Python console:
    #    help(unreal.MovieGraphValueContainer)
    #
    # In this case, we're setting the value of a MovieGraphNamedResolution struct. Structs are the one case where there's
    # not a specific set_value_struct() method to call, so set_value_serialized_string() must be called instead. This takes
    # the serialized value of the property, and can be used with any data type, not just structs.
    #
    # In this example, we're using the return value of named_resolution_from_profile() -- a MovieGraphNamedResolution
    # struct -- and calling its export_text() method, which returns the serialized representation of the struct.
    variable_overrides.set_value_serialized_string(custom_output_res_variable, 
        unreal.MovieGraphLibrary.named_resolution_from_profile("720p (HD)").export_text())

    # Enable override toggle. The variable's value will not take effect unless the enable state is true.
    variable_overrides.set_variable_assignment_enable_state(variable, True)

    print("Set value of CustomOutputRes to '720p (HD)'")


@staticmethod
def get_variable_overrides(job: unreal.MoviePipelineExecutorJob):
    """
    Prints out information about all variable overrides. Additionally, this prints out more detailed information on
    the 'OutputDirectory' variable if it's present (which should be a DirectoryPath struct).
    
    Args:
        job (unreal.MoviePipelineExecutorJob): the Pipeline Job with the variable overrides
    """
    graph = job.get_graph_preset()
    variables = graph.get_variables()

    # See set_variable_overrides() for more explanation about variable overrides
    variable_overrides = job.get_or_create_variable_overrides(graph)

    # Print out information about each variable. A variable override will not take effect unless it is enabled (which is
    # equivalent to the override checkbox in the UI).
    for variable in variables:
        # Note that get_member_name() retrieves the variable name, which is *not* the same as get_name()
        variable_name = variable.get_member_name()

        value_type = variable_overrides.get_value_type(variable)

        if value_type == unreal.MovieGraphValueType.BOOL:
            variable_value = variable_overrides.get_value_bool(variable)
        elif value_type == unreal.MovieGraphValueType.STRING:
            variable_value = variable_overrides.get_value_string(variable)
        elif value_type == unreal.MovieGraphValueType.INT32:
            variable_value = variable_overrides.get_value_int32(variable)
        else:
            # Etc. Use the appropriate get_*() for the variable's type.
            print("Example doesn't cover the the type for this variable: %s" % variable_name)
            continue

        enable_state_value = variable_overrides.get_variable_assignment_enable_state(variable)
        variable_type = variable_overrides.get_value_type(variable)

        # For more complex types, you can get the type object. For variables that store the value of a class, enum, or struct,
        # the get_value_type_object() will return the underlying class, enum, or struct type respectively.
        variable_type_object = None
        if variable_type in [unreal.MovieGraphValueType.CLASS, unreal.MovieGraphValueType.ENUM, unreal.MovieGraphValueType.STRUCT]:
            variable_type_object = variable_overrides.get_value_type_object(output_directory_variable)

        print("Found variable override. Name: %s, Value: %s, Enable State: %s, Type: %s, Value Type Object: %s" % (
            variable_name, variable_value, enable_state_value, variable_type, variable_type_object))

    # For structs, getting their value can be tricky because there's not a specific get_value_struct() method; the
    # get_value_serialized_string() method needs to be used instead. This fetches the serialized value of the struct.
    # An existing struct can "import" the serialized value, then the struct can be used like normal. In this example, we'll
    # use a struct variable that stores the output directory (which has the type of DirectoryPath).
    output_directory_variable = graph.get_variable_by_name("OutputDirectory")
    if output_directory_variable:
        serialized_value = variable_overrides.get_value_serialized_string(output_directory_variable)
        output_directory_struct = unreal.DirectoryPath()
        output_directory_struct.import_text(serialized_value)
        print("Output directory path: %s" % output_directory_struct.path)
    else:
        print("Could not find 'OutputDirectory' variable")


@staticmethod
def duplicate_queue(pipeline_queue: unreal.MoviePipelineQueue):
    """
    Duplicating a queue is desirable in an interactive session, especially when you 
    want to modify a copy of the Queue Asset instead of altering the original one.
    Args:
        queue (unreal.MoviePipelineQueue): The Queue which we want to duplicate
    """
    new_queue = unreal.MoviePipelineQueue()
    new_queue.copy_from(pipeline_queue)
    pipeline_queue = new_queue
    return pipeline_queue 


@staticmethod
def advanced_job_operations(job: unreal.MoviePipelineExecutorJob):
    """
    Wrapper function that runs the following functions on the current job
    - set_job_parameters
    - set_variable_overrides
    - get_variable_overrides
    - set_global_output_settings_node

    Args:
        job (unreal.MoviePipelineJob): The current processed Queue Job
    """
    if not job.get_graph_preset():
        unreal.log("This Job doesn't have a graph type preset, add a graph preset to the job to test this function")
        return

    # Set Job parameters such as Author/Level/LevelSequence
    set_job_parameters(job)

    # Set variable overrides on the job
    set_variable_overrides(job)
    
    # Get information about the variable overrides available on the job
    get_variable_overrides(job)
    
    # Set attributes on the actual graph's nodes directly, this is like
    # setting the default values
    set_global_output_settings_node(job)


@staticmethod
def traverse_graph_config(graph: unreal.MovieGraphConfig):
    """Demonstrates how we can use depth first search to visit all the nodes starting
    from the "Globals" pin and navigating our way to the left until all nodes are 
    exhausted

    Args:
        graph (unreal.MovieGraphConfig): The graph to operate on
    """
    visited = set()

    def dfs(node, visisted=None):
        visited.add(node.get_name())
        
        # Nodes can have different number of input nodes and names which we need to collect
        if isinstance(node, unreal.MovieGraphSubgraphNode) or isinstance(node, unreal.MovieGraphOutputNode):
            pins = [node.get_input_pin("Globals"), node.get_input_pin("Input")]
        elif isinstance(node, unreal.MovieGraphBranchNode):
            pins = [node.get_input_pin("True"), node.get_input_pin("False")]
        elif isinstance(node, unreal.MovieGraphSelectNode):
            pins = [node.get_input_pin("Default")]

        else:
            pins = [node.get_input_pin("")]

        # Iterate over the found pins
        for pin in pins:
            if pin:
                for neighbour in pin.get_connected_nodes():
                    dfs(neighbour, visited)
