# Copyright Epic Games, Inc. All Rights Reserved.
# These examples showcase how to use scripting with movie render graph
import unreal
import MovieGraphCreateConfigExample as graph_helper
import MovieGraphEditorExampleHelpers

# USAGE:
#   - Requires the "Python Editor Script Plugin" to be enabled in your project.
#   In the main() function, you'll find examples demonstrating how to load and 
#   modify movie graphs, overwrite exposed variables and perform other 
#   operations related to the Movie Render Graph
#
#   Make sure to change the "Assets to load" section to point to your own 
#   Movie Render Queue and Movie Graph Config Assets


'''
Python Globals to keep the UObjects alive through garbage collection.
Must be deleted by hand on completion.
'''
subsystem = None
executor = None


def render_queue(queue_to_load: unreal.MoviePipelineQueue=None, 
                 graph_to_load: unreal.MovieGraphConfig=None):
    """
    This example demonstrates how to:
    - load a queue or use the current queue
    - Set a graph Config as a Preset
    - Add a simple executor started callback 
    - Add a simple executor finished callback 
    """

    # Get the subsystem to interact with the Movie Pipeline Queue
    subsystem = unreal.get_editor_subsystem(unreal.MoviePipelineQueueSubsystem)

    # Load the provided Queue Asset if provided, otherwise use the active Queue  
    if queue_to_load:
        if subsystem.load_queue(queue_to_load, prompt_on_replacing_dirty_queue=False):
            unreal.log("Loaded specified queue")
    
    # Get a reference to the active Queue
    pipeline_queue = subsystem.get_queue()
    
    if not pipeline_queue.get_jobs():
        unreal.log("There are no jobs in the Queue.")
        return
        
    # For each job we can start modifying the job parameters such as accessing
    # the graph preset of the job, modifying values
    for job in pipeline_queue.get_jobs():

        #Make sure we are working with a job graph config for this example
        if not job.get_graph_preset():
            unreal.log("A Graph Config needs to be specified for this example)")
            return
        
        if graph_to_load:
            job.set_graph_preset(graph_to_load)

        # A collection of job operation examples can be found in 
        MovieGraphEditorExampleHelpers.advanced_job_operations(job)


    # We are using the globals keyword to indicate that the executor belongs to the 
    # global scope to avoid it being garbage collected after the job is finished rendering
    global executor

    executor = unreal.MoviePipelinePIEExecutor(subsystem)
    
    # Before a render starts, execute the callback on_individual_job_started_callback
    executor.on_individual_job_started_delegate.add_callable_unique(
        MovieGraphEditorExampleHelpers.on_job_started_callback
    )
    
    # When the render jobs are done, execute the callback on_queue_finished_callback
    executor.on_executor_finished_delegate.add_callable_unique(
        MovieGraphEditorExampleHelpers.on_queue_finished_callback
    )
 
    # Start Rendering, similar to pressing "Render" in the UI, 
    subsystem.render_queue_with_executor_instance(executor)


def allocate_render_job(config_to_load: unreal.MovieGraphConfig=None):
    '''
    Allocates new job and populates it's parameters before kicking off a render
    '''
    # Get The current Queue
    subsytem = unreal.get_editor_subsystem(unreal.MoviePipelineQueueSubsystem)
    pipeline_queue = subsytem.get_queue()

    # Delete existing jobs to clear the current Queue
    pipeline_queue.delete_all_jobs()

    job = pipeline_queue.allocate_new_job(unreal.MoviePipelineExecutorJob)
    
    # To override Job Parameters check 
    # MovieGraphEditorExampleHelpers.set_job_parameters(job)
    
    if config_to_load:
        job.set_graph_preset(config_to_load)

    subsystem = unreal.get_editor_subsystem(unreal.MoviePipelineQueueSubsystem)
    executor = unreal.MoviePipelinePIEExecutor(subsystem)
    subsystem.render_queue_with_executor_instance(executor)


def render_queue_minimal():
    '''This an MVP example on how to render a Queue which already has jobs allocated
    A more exhaustive example covering overrides can be found in the function render_queue
    '''
    subsystem = unreal.get_editor_subsystem(unreal.MoviePipelineQueueSubsystem)
    executor = unreal.MoviePipelinePIEExecutor(subsystem)
    subsystem.render_queue_with_executor_instance(executor)


def main():
    """We are showcasing how to render a queue (render_queue) and also how to create 
    a job from scratch without initially creating a queue. (allocate_render_job) 
    Run these examples independently. 
    """

    # Creates A Graph Asset called "Example" in /Game/MyTests/IntermediateConfig 
    # that exposes the output resolution as a user variable 
    created_graph = graph_helper.CreateIntermediateConfig()

    # Render a Queue with a saved Queue Asset, Add Executor Callbacks, you can pass
    # a Movie Render Queue Asset or just make sure you have jobs in a Movie Render Queue
    render_queue()

    # This function creates a job in the Queue by allocating a new MoviePipelineJob, 
    # You can also pass in config_to_load to set a graph config and finally start's the render
    allocate_render_job()

if __name__ == "__main__":
    unreal.log("Check the main() function for examples")
