class MoviePipelineExecutorBase(Object):
    r"""
    A Movie Pipeline Executor is responsible for executing an array of Movie Pipelines,
    and (optionally) reporting progress back for the movie pipelines. The entire array
    is passed at once to allow the implementations to choose how to split up the work.
    By default we provide a local execution (UMoviePipelineLocalExecutor) which works
    on them serially, but you can create an implementation of this class, change the
    default in the Project Settings and use your own distribution logic. For example,
    you may want to distribute the work to multiple computers over a network, which
    may involve running command line options on each machine to sync the latest content
    from the project before the execution starts.
    
    **C++ Source:**
    
    - **Plugin**: MovieRenderPipeline
    - **Module**: MovieRenderPipelineCore
    - **File**: MoviePipelineExecutor.h
    
    **Editor Properties:** (see get_editor_property/set_editor_property)
    
    - ``debug_widget_class`` (type(Class)):  [Read-Write]
    - ``http_response_recieved_delegate`` (MoviePipelineHttpResponseRecieved):  [Read-Write] If an HTTP Request has been made and a response returned, the returned response will be broadcast on this event.
    - ``on_executor_errored_delegate`` (OnMoviePipelineExecutorErrored):  [Read-Write] Called when an individual job reports a warning/error. Jobs are considered fatal
      if the severity was bad enough to abort the job (missing sequence, write failure, etc.)
      
      Exposed for Blueprints/Python. Called at the same time as the native one.
    - ``on_executor_finished_delegate`` (OnMoviePipelineExecutorFinished):  [Read-Write] Called when the Executor has finished all jobs. Reports success if no jobs
      had fatal errors. Subscribe to the error delegate for more information about
      any errors.
      
      Exposed for Blueprints/Python. Called at the same time as the native one.
    - ``socket_message_recieved_delegate`` (MoviePipelineSocketMessageRecieved):  [Read-Write] If this executor has been configured to connect to a socket, this event will be called each time the socket recieves something.
      The message response expected from the server should have a 4 byte (int32) size prefix for the string to specify how much data
      we should expect. This delegate will not get invoked until we recieve that many bytes from the socket connection to avoid broadcasting
      partial messages.
    - ``target_pipeline_class`` (type(Class)):  [Read-Write] Which Pipeline Class should be created by this Executor. May be null.
    - ``user_data`` (str):  [Read-Write] Arbitrary data that can be associated with the executor. Not used by default implementations, nor read.
      This can be used to attach third party metadata such as job ids from remote farms.
    """
    @property
    def on_executor_finished_delegate(self) -> OnMoviePipelineExecutorFinished:
        r"""
        (OnMoviePipelineExecutorFinished):  [Read-Write] Called when the Executor has finished all jobs. Reports success if no jobs
        had fatal errors. Subscribe to the error delegate for more information about
        any errors.
        
        Exposed for Blueprints/Python. Called at the same time as the native one.
        """
        ...
    @on_executor_finished_delegate.setter
    def on_executor_finished_delegate(self, value: OnMoviePipelineExecutorFinished) -> None:
        ...
    @property
    def on_executor_errored_delegate(self) -> OnMoviePipelineExecutorErrored:
        r"""
        (OnMoviePipelineExecutorErrored):  [Read-Write] Called when an individual job reports a warning/error. Jobs are considered fatal
        if the severity was bad enough to abort the job (missing sequence, write failure, etc.)
        
        Exposed for Blueprints/Python. Called at the same time as the native one.
        """
        ...
    @on_executor_errored_delegate.setter
    def on_executor_errored_delegate(self, value: OnMoviePipelineExecutorErrored) -> None:
        ...
    @property
    def socket_message_recieved_delegate(self) -> MoviePipelineSocketMessageRecieved:
        r"""
        (MoviePipelineSocketMessageRecieved):  [Read-Write] If this executor has been configured to connect to a socket, this event will be called each time the socket recieves something.
        The message response expected from the server should have a 4 byte (int32) size prefix for the string to specify how much data
        we should expect. This delegate will not get invoked until we recieve that many bytes from the socket connection to avoid broadcasting
        partial messages.
        """
        ...
    @socket_message_recieved_delegate.setter
    def socket_message_recieved_delegate(self, value: MoviePipelineSocketMessageRecieved) -> None:
        ...
    @property
    def http_response_recieved_delegate(self) -> MoviePipelineHttpResponseRecieved:
        r"""
        (MoviePipelineHttpResponseRecieved):  [Read-Write] If an HTTP Request has been made and a response returned, the returned response will be broadcast on this event.
        """
        ...
    @http_response_recieved_delegate.setter
    def http_response_recieved_delegate(self, value: MoviePipelineHttpResponseRecieved) -> None:
        ...
    @property
    def debug_widget_class(self) -> Class:
        r"""
        (type(Class)):  [Read-Write]
        """
        ...
    @debug_widget_class.setter
    def debug_widget_class(self, value: Class) -> None:
        ...
    @property
    def user_data(self) -> str:
        r"""
        (str):  [Read-Write] Arbitrary data that can be associated with the executor. Not used by default implementations, nor read.
        This can be used to attach third party metadata such as job ids from remote farms.
        """
        ...
    @user_data.setter
    def user_data(self, value: str) -> None:
        ...
    @property
    def target_pipeline_class(self) -> Class:
        r"""
        (type(Class)):  [Read-Write] Which Pipeline Class should be created by this Executor. May be null.
        """
        ...
    @target_pipeline_class.setter
    def target_pipeline_class(self, value: Class) -> None:
        ...
    def set_status_progress(self, progress: float) -> None:
        r"""
        x.set_status_progress(progress) -> None
        Set the progress of this Executor. Does nothing in default implementations, but a useful shorthand
        for implementations to broadcast progress updates, ie: You can call SetStatusProgress as your executor
        changes progress, and override the SetStatusProgress function to make it actually do something (such as
        printing to a log or updating a third party API).
        
        Does not necessairly reflect the overall progress of the work, it is an arbitrary 0-1 value that
        can be used to indicate different things (depending on implementation).
        
        For C++ implementations override `virtual bool SetStatusProgress_Implementation() override`
        For Python/BP implementations override
        unreal.ufunction(override=True): def set_status_progress(self, inStatus):
        
        Args:
            progress (float): The progress (0-1 range) the executor should have.
        """
        ...
    def set_status_message(self, status: str) -> None:
        r"""
        x.set_status_message(status) -> None
        Set the status of this Executor. Does nothing in default implementations, but a useful shorthand
        for implementations to broadcast status updates, ie: You can call SetStatusMessage as your executor
        changes state, and override the SetStatusMessage function to make it actually do something (such as
        printing to a log or updating a third party API).
        
        For C++ implementations override `virtual bool SetStatusMessage_Implementation() override`
        For Python/BP implementations override
        unreal.ufunction(override=True): def set_status_message(self, inStatus):
        
        Args:
            status (str): The status message you wish the executor to have.
        """
        ...
    def set_movie_pipeline_class(self, pipeline_class: Class) -> None:
        r"""
        x.set_movie_pipeline_class(pipeline_class) -> None
        Specify which MoviePipeline class type should be created by this executor when processing jobs.
        
        Args:
            pipeline_class (type(Class)):
        """
        ...
    def send_socket_message(self, message: str) -> bool:
        r"""
        x.send_socket_message(message) -> bool
        Sends a socket message if the socket is currently connected. Messages back will happen in the OnSocketMessageRecieved event.
        
        Args:
            message (str): The message to send. This will be sent over the socket (if connected) with a 4 byte (int32) size prefix on the message so the recieving end knows how much data to recieve before considering it done. This prevents accidentally chopping json strings in half.
        
        Returns:
            bool: True if the message was sent succesfully.
        """
        ...
    def send_http_request(self, url: str, verb: str, message: str, headers: Map[str, str]) -> int:
        r"""
        x.send_http_request(url, verb, message, headers) -> int32
        Sends a asynchronous HTTP request. Responses will be returned in the the OnHTTPResponseRecieved event.
        
        Args:
            url (str): The URL to send the request to.
            verb (str): The HTTP verb for the request. GET, PUT, POST, etc.
            message (str): The content of the request.
            headers (Map[str, str]): Headers that should be set on the request.
        
        Returns:
            int32: Returns an index for the request. This index will be provided in the OnHTTPResponseRecieved event so you can make multiple HTTP requests at once and tell them apart when you recieve them, even if they come out of order.
        """
        ...
    def on_executor_finished_impl(self) -> None:
        r"""
        x.on_executor_finished_impl() -> None
        This should be called when the Executor has finished executing all of the things
        it has been asked to execute. This should be called in the event of a failure as
        well, and passing in false for success to allow the caller to know failure. Errors
        should be broadcast on the error delegate, so this is just a handy way to know at
        the end without having to track it yourself.
        """
        ...
    def on_executor_errored_impl(self, errored_pipeline: MoviePipeline, fatal: bool, error_reason: Text) -> None:
        r"""
        x.on_executor_errored_impl(errored_pipeline, fatal, error_reason) -> None
        On Executor Errored Impl
        
        Args:
            errored_pipeline (MoviePipeline): 
            fatal (bool): 
            error_reason (Text):
        """
        ...
    def on_begin_frame(self) -> None:
        r"""
        x.on_begin_frame() -> None
        Called once at the beginning of each engine frame.
        
        For C++ implementations override `virtual bool OnBeginFrame_Implementation() override`
        For Python/BP implementations override
        unreal.ufunction(override=True): def on_begin_frame(self): super().on_begin_frame()
        """
        ...
    def is_socket_connected(self) -> bool:
        r"""
        x.is_socket_connected() -> bool
        Returns true if the socket is currently connected, false otherwise. Call ConnectSocket to attempt a connection.
        
        Returns:
            bool:
        """
        ...
    def is_rendering(self) -> bool:
        r"""
        x.is_rendering() -> bool
        Report the current state of the executor. This is used to know if we can call Execute again.
        
        For C++ implementations override `virtual bool IsRendering_Implementation() const override`
        For Python/BP implementations override
        unreal.ufunction(override=True): def is_rendering(self): return ?
        
        Returns:
            bool: True if the executor is currently working on a queue to produce a render.
        """
        ...
    def get_status_progress(self) -> float:
        r"""
        x.get_status_progress() -> float
        Get the current progress as last set by SetStatusProgress. 0 by default.
        
        For C++ implementations override `virtual float GetStatusProgress_Implementation() override`
        For Python/BP implementations override
        unreal.ufunction(override=True): def get_status_progress(self): return ?
        
        Returns:
            float:
        """
        ...
    def get_status_message(self) -> str:
        r"""
        x.get_status_message() -> str
        Get the current status message for this job. May be an empty string.
        
        For C++ implementations override `virtual FString GetStatusMessage_Implementation() override`
        For Python/BP implementations override
        unreal.ufunction(override=True): def get_status_message(self): return ?
        
        Returns:
            str:
        """
        ...
    def execute(self, pipeline_queue: MoviePipelineQueue) -> None:
        r"""
        x.execute(pipeline_queue) -> None
        Execute the provided Queue. You are responsible for deciding how to handle each job
        in the queue and processing them. OnExecutorFinished should be called when all jobs
        are completed, which can report both success, warning, cancel, or error.
        
        For C++ implementations override `virtual void Execute_Implementation() const override`
        For Python/BP implementations override
        unreal.ufunction(override=True): def execute(self):
        
        Args:
            pipeline_queue (MoviePipelineQueue): The queue that this should process all jobs for. This can be null when using certain combination of command line render flags/scripting.
        """
        ...
    def disconnect_socket(self) -> None:
        r"""
        x.disconnect_socket() -> None
        * Disconnects the socket (if currently connected.)
        """
        ...
    def connect_socket(self, host_name: str, port: int) -> bool:
        r"""
        x.connect_socket(host_name, port) -> bool
        Attempts to connect a socket to the specified host and port. This is a blocking call.
        
        Args:
            host_name (str): The host name as to connect to such as "127.0.0.1"
            port (int32): The port to attempt to connect to the host on.
        
        Returns:
            bool: True if the socket was succesfully connected to the given host and port.
        """
        ...
    def cancel_current_job(self) -> None:
        r"""
        x.cancel_current_job() -> None
        Abort the currently executing job.
        """
        ...
    def cancel_all_jobs(self) -> None:
        r"""
        x.cancel_all_jobs() -> None
        Abort the currently executing job and skip all other jobs.
        """
        ...

class MoviePipelineExecutorJob(Object):
    r"""
    A particular job within the Queue
    
    **C++ Source:**
    
    - **Plugin**: MovieRenderPipeline
    - **Module**: MovieRenderPipelineCore
    - **File**: MoviePipelineQueue.h
    
    **Editor Properties:** (see get_editor_property/set_editor_property)
    
    - ``author`` (str):  [Read-Write] (Optional) If left blank, will default to system username. Can be shown in burn in as a first point of contact about the content.
    - ``comment`` (str):  [Read-Write] (Optional) If specified, will be shown in the burn in to allow users to keep track of notes about a render.
    - ``console_variable_overrides`` (Array[MoviePipelineConsoleVariableEntry]):  [Read-Write] (Optional) Console variable overrides which are applied after cvars set via nodes. Only applies to graph-based configs.
    - ``graph_variable_assignments`` (Array[MovieJobVariableAssignmentContainer]):  [Read-Write] Overrides on the variables in the graph (and subgraphs) associated with this job.
    - ``job_name`` (str):  [Read-Write] (Optional) Name of the job. Shown on the default burn-in.
    - ``map`` (SoftObjectPath):  [Read-Write] Which map should this job render on
    - ``sequence`` (SoftObjectPath):  [Read-Write] Which sequence should this job render?
    - ``shot_info`` (Array[MoviePipelineExecutorShot]):  [Read-Write] (Optional) Shot specific information. If a shot is missing from this list it will assume to be enabled and will be rendered.
    - ``user_data`` (str):  [Read-Write] Arbitrary data that can be associated with the job. Not used by default implementations, nor read.
      This can be used to attach third party metadata such as job ids from remote farms.
      Not shown in the user interface.
    """
    @property
    def job_name(self) -> str:
        r"""
        (str):  [Read-Write] (Optional) Name of the job. Shown on the default burn-in.
        """
        ...
    @job_name.setter
    def job_name(self, value: str) -> None:
        ...
    @property
    def sequence(self) -> SoftObjectPath:
        r"""
        (SoftObjectPath):  [Read-Write] Which sequence should this job render?
        """
        ...
    @sequence.setter
    def sequence(self, value: SoftObjectPath) -> None:
        ...
    @property
    def map(self) -> SoftObjectPath:
        r"""
        (SoftObjectPath):  [Read-Write] Which map should this job render on
        """
        ...
    @map.setter
    def map(self, value: SoftObjectPath) -> None:
        ...
    @property
    def author(self) -> str:
        r"""
        (str):  [Read-Write] (Optional) If left blank, will default to system username. Can be shown in burn in as a first point of contact about the content.
        """
        ...
    @author.setter
    def author(self, value: str) -> None:
        ...
    @property
    def comment(self) -> str:
        r"""
        (str):  [Read-Write] (Optional) If specified, will be shown in the burn in to allow users to keep track of notes about a render.
        """
        ...
    @comment.setter
    def comment(self, value: str) -> None:
        ...
    @property
    def shot_info(self) -> Array[MoviePipelineExecutorShot]:
        r"""
        (Array[MoviePipelineExecutorShot]):  [Read-Write] (Optional) Shot specific information. If a shot is missing from this list it will assume to be enabled and will be rendered.
        """
        ...
    @shot_info.setter
    def shot_info(self, value: Array[MoviePipelineExecutorShot]) -> None:
        ...
    @property
    def user_data(self) -> str:
        r"""
        (str):  [Read-Write] Arbitrary data that can be associated with the job. Not used by default implementations, nor read.
        This can be used to attach third party metadata such as job ids from remote farms.
        Not shown in the user interface.
        """
        ...
    @user_data.setter
    def user_data(self, value: str) -> None:
        ...
    @property
    def console_variable_overrides(self) -> Array[MoviePipelineConsoleVariableEntry]:
        r"""
        (Array[MoviePipelineConsoleVariableEntry]):  [Read-Write] (Optional) Console variable overrides which are applied after cvars set via nodes. Only applies to graph-based configs.
        """
        ...
    @console_variable_overrides.setter
    def console_variable_overrides(self, value: Array[MoviePipelineConsoleVariableEntry]) -> None:
        ...
    def set_status_progress(self, progress: float) -> None:
        r"""
        x.set_status_progress(progress) -> None
        Set the progress of this job to the given value. If a positive value is provided
        the UI will show the progress bar, while a negative value will make the UI show the
        status message instead. Progress and Status Message are cosmetic and dependent on the
        executor to update. Similar to the UMoviePipelineExecutor::SetStatusProgress function,
        but at a per-job level basis instead.
        
        For C++ implementations override `virtual void SetStatusProgress_Implementation() override`
        For Python/BP implementations override
        unreal.ufunction(override=True): def set_status_progress(self, inProgress):
        
        Args:
            progress (float): The progress (0-1 range) the executor should have.
        """
        ...
    def set_status_message(self, status: str) -> None:
        r"""
        x.set_status_message(status) -> None
        Set the status of this job to the given value. This will be shown on the UI if progress
        is set to a value less than zero. If progress is > 0 then the progress bar will be shown
        on the UI instead. Progress and Status Message are cosmetic and dependent on the
        executor to update. Similar to the UMoviePipelineExecutor::SetStatusMessage function,
        but at a per-job level basis instead.
        
        For C++ implementations override `virtual void SetStatusMessage_Implementation() override`
        For Python/BP implementations override
        unreal.ufunction(override=True): def set_status_message(self, inStatus):
        
        Args:
            status (str): The status message you wish the executor to have.
        """
        ...
    def set_preset_origin(self, preset: MoviePipelinePrimaryConfig) -> None:
        r"""
        x.set_preset_origin(preset) -> None
        Set Preset Origin
        
        Args:
            preset (MoviePipelinePrimaryConfig):
        """
        ...
    def set_is_enabled(self, enabled: bool) -> None:
        r"""
        x.set_is_enabled(enabled) -> None
        Set the job to be enabled/disabled. This is exposed to the user in the Queue UI
        so they can disable a job after loading a queue to skip trying to run it.
        
        For C++ implementations override `virtual void SetIsEnabled_Implementation() override`
        For Python/BP implementations override
        unreal.ufunction(override=True): def set_is_enabled(self, isEnabled):
        
        Args:
            enabled (bool): True if the job should be enabled and rendered.
        """
        ...
    def set_graph_preset(self, graph_preset: MovieGraphConfig, update_variable_assignments: bool = True) -> None:
        r"""
        x.set_graph_preset(graph_preset, update_variable_assignments=True) -> None
        Sets the graph-style preset that this job will use. Note that this will cause the graph to switch over to using
        graph-style configuration if it is not already using it.
        
        Args:
            graph_preset (MovieGraphConfig): The graph preset to assign to the job.
            update_variable_assignments (bool): Set to false if variable assignments should NOT be automatically updated to reflect the graph preset being used. This is normally not what you want and should be used with caution.
        """
        ...
    def set_consumed(self, consumed: bool) -> None:
        r"""
        x.set_consumed(consumed) -> None
        Set the job to be consumed. A consumed job is disabled in the UI and should not be
        submitted for rendering again. This allows jobs to be added to a queue, the queue
        submitted to a remote farm (consume the jobs) and then more jobs to be added and
        the second submission to the farm won't re-submit the already in-progress jobs.
        
        Jobs can be unconsumed when the render finishes to re-enable editing.
        
        For C++ implementations override `virtual void SetConsumed_Implementation() override`
        For Python/BP implementations override
        unreal.ufunction(override=True): def set_consumed(self, isConsumed):
        
        Args:
            consumed (bool): True if the job should be consumed and disabled for editing in the UI.
        """
        ...
    def set_configuration(self, preset: MoviePipelinePrimaryConfig) -> None:
        r"""
        x.set_configuration(preset) -> None
        Set Configuration
        
        Args:
            preset (MoviePipelinePrimaryConfig):
        """
        ...
    def on_duplicated(self) -> None:
        r"""
        x.on_duplicated() -> None
        Should be called to clear status and user data after duplication so that jobs stay
        unique and don't pick up ids or other unwanted behavior from the pareant job.
        
        For C++ implementations override `virtual bool OnDuplicated_Implementation() override`
        For Python/BP implementations override
        unreal.ufunction(override=True): def on_duplicated(self):
        """
        ...
    def is_using_graph_configuration(self) -> bool:
        r"""
        x.is_using_graph_configuration() -> bool
        Returns true if this job is using graph-style configuration, else false.
        
        Returns:
            bool:
        """
        ...
    def is_enabled(self) -> bool:
        r"""
        x.is_enabled() -> bool
        Gets whether or not the job has been marked as being enabled.
        
        For C++ implementations override `virtual bool IsEnabled_Implementation() const override`
        For Python/BP implementations override
        unreal.ufunction(override=True): def is_enabled(self): return ?
        
        Returns:
            bool:
        """
        ...
    def is_consumed(self) -> bool:
        r"""
        x.is_consumed() -> bool
        Gets whether or not the job has been marked as being consumed. A consumed job is not editable
        in the UI and should not be submitted for rendering as it is either already finished or
        already in progress.
        
        For C++ implementations override `virtual bool IsConsumed_Implementation() override`
        For Python/BP implementations override
        unreal.ufunction(override=True): def is_consumed(self): return ?
        
        Returns:
            bool:
        """
        ...
    def get_status_progress(self) -> float:
        r"""
        x.get_status_progress() -> float
        Get the current progress as last set by SetStatusProgress. 0 by default.
        
        For C++ implementations override `virtual float GetStatusProgress_Implementation() override`
        For Python/BP implementations override
        unreal.ufunction(override=True): def get_status_progress(self): return ?
        
        Returns:
            float:
        """
        ...
    def get_status_message(self) -> str:
        r"""
        x.get_status_message() -> str
        Get the current status message for this job. May be an empty string.
        
        For C++ implementations override `virtual FString GetStatusMessage_Implementation() override`
        For Python/BP implementations override
        unreal.ufunction(override=True): def get_status_message(self): return ?
        
        Returns:
            str:
        """
        ...
    def get_preset_origin(self) -> MoviePipelinePrimaryConfig:
        r"""
        x.get_preset_origin() -> MoviePipelinePrimaryConfig
        Gets the preset for this job, but only if the preset has not been modified. If it has been modified, or the preset
        no longer exists, returns nullptr.
        see: GetConfiguration()
        
        Returns:
            MoviePipelinePrimaryConfig:
        """
        ...
    def get_or_create_variable_overrides(self, graph: MovieGraphConfig) -> MovieJobVariableAssignmentContainer:
        r"""
        x.get_or_create_variable_overrides(graph) -> MovieJobVariableAssignmentContainer
        This method will return you the object which contains variable overrides for the Primary Graph assigned to this job. You need to provide
        a pointer to the exact graph you want (as the Primary Graph may contain sub-graphs, and those sub-graphs can have their own variables),
        though this will be the same as GetGraphPreset() if you're not using any sub-graphs, or your variables only exist on the Primary graph.
        
        If you want to override a variable on the primary graph but only for a specific shot, you should get the UMoviePipelineExecutorShot and
        call that classes version of this function, except passing True for the extra boolean.  See comment on that function for more details.
        
        Args:
            graph (MovieGraphConfig): The graph asset to get the Job Override values for. Should be the graph the variables you want to edit are defined on, which can either be the primary graph (GetGraphPreset()) or one of the sub-graphs it points to (as sub-graphs can contain their own variables which are all shown at the top level job in the Editor UI).
        
        Returns:
            MovieJobVariableAssignmentContainer: A container object which holds a copy of the variables for the specified Graph Asset that can be used to override their values on jobs without actually editing the default asset.
        """
        ...
    def get_graph_preset(self) -> MovieGraphConfig:
        r"""
        x.get_graph_preset() -> MovieGraphConfig
        Gets the graph-style preset that this job is using. If the job is not using a graph-style preset, returns nullptr.
        see: GetPresetOrigin()
        
        Returns:
            MovieGraphConfig:
        """
        ...
    def get_configuration(self) -> MoviePipelinePrimaryConfig:
        r"""
        x.get_configuration() -> MoviePipelinePrimaryConfig
        Gets the configuration for the job. This configuration is either standalone (not associated with any preset), or
        contains a copy of the preset origin plus any modifications made on top of it. If the preset that this
        configuration was originally based on no longer exists, this configuration will still be valid.
        see: GetPresetOrigin()
        
        Returns:
            MoviePipelinePrimaryConfig:
        """
        ...

class MoviePipelineExecutorShot(Object):
    r"""
    This class represents a segment of work within the Executor Job. This should be owned
    by the UMoviePipelineExecutorJob and can be created before the movie pipeline starts to
    configure some aspects about the segment (such as disabling it). When the movie pipeline
    starts, it will use the already existing ones, or generate new ones as needed.
    
    **C++ Source:**
    
    - **Plugin**: MovieRenderPipeline
    - **Module**: MovieRenderPipelineCore
    - **File**: MoviePipelineQueue.h
    
    **Editor Properties:** (see get_editor_property/set_editor_property)
    
    - ``console_variable_overrides`` (Array[MoviePipelineConsoleVariableEntry]):  [Read-Write] (Optional) Console variable overrides which are applied after cvars set via nodes. Only applies to graph-based configs.
    - ``enabled`` (bool):  [Read-Write] Does the user want to render this shot?
    - ``graph_variable_assignments`` (Array[MovieJobVariableAssignmentContainer]):  [Read-Write] Overrides on the variables in the graph (and subgraphs) associated with this job.
    - ``inner_name`` (str):  [Read-Write] The name of the camera cut section that this shot represents. Can be empty.
    - ``outer_name`` (str):  [Read-Write] The name of the shot section that contains this shot. Can be empty.
    - ``primary_graph_variable_assignments`` (Array[MovieJobVariableAssignmentContainer]):  [Read-Write] Overrides on the variables in the primary graph (and its subgraphs) associated with this job.
    - ``sidecar_cameras`` (Array[MoviePipelineSidecarCamera]):  [Read-Write] List of cameras to render for this shot. Only used if the setting flag is set in the Camera setting.
    """
    @property
    def enabled(self) -> bool:
        r"""
        (bool):  [Read-Write] Does the user want to render this shot?
        """
        ...
    @enabled.setter
    def enabled(self, value: bool) -> None:
        ...
    @property
    def outer_name(self) -> str:
        r"""
        (str):  [Read-Write] The name of the shot section that contains this shot. Can be empty.
        """
        ...
    @outer_name.setter
    def outer_name(self, value: str) -> None:
        ...
    @property
    def inner_name(self) -> str:
        r"""
        (str):  [Read-Write] The name of the camera cut section that this shot represents. Can be empty.
        """
        ...
    @inner_name.setter
    def inner_name(self, value: str) -> None:
        ...
    @property
    def sidecar_cameras(self) -> Array[MoviePipelineSidecarCamera]:
        r"""
        (Array[MoviePipelineSidecarCamera]):  [Read-Write] List of cameras to render for this shot. Only used if the setting flag is set in the Camera setting.
        """
        ...
    @sidecar_cameras.setter
    def sidecar_cameras(self, value: Array[MoviePipelineSidecarCamera]) -> None:
        ...
    @property
    def console_variable_overrides(self) -> Array[MoviePipelineConsoleVariableEntry]:
        r"""
        (Array[MoviePipelineConsoleVariableEntry]):  [Read-Write] (Optional) Console variable overrides which are applied after cvars set via nodes. Only applies to graph-based configs.
        """
        ...
    @console_variable_overrides.setter
    def console_variable_overrides(self, value: Array[MoviePipelineConsoleVariableEntry]) -> None:
        ...
    def should_render(self) -> bool:
        r"""
        x.should_render() -> bool
        Returns whether this should should be rendered
        
        Returns:
            bool:
        """
        ...
    def set_status_progress(self, progress: float) -> None:
        r"""
        x.set_status_progress(progress) -> None
        Set the progress of this shot to the given value. If a positive value is provided
        the UI will show the progress bar, while a negative value will make the UI show the
        status message instead. Progress and Status Message are cosmetic and dependent on the
        executor to update. Similar to the UMoviePipelineExecutor::SetStatusProgress function,
        but at a per-job level basis instead.
        
        For C++ implementations override `virtual void SetStatusProgress_Implementation() override`
        For Python/BP implementations override
        unreal.ufunction(override=True): def set_status_progress(self, inStatus):
        
        Args:
            progress (float): The progress (0-1 range) the executor should have.
        """
        ...
    def set_status_message(self, status: str) -> None:
        r"""
        x.set_status_message(status) -> None
        Set the status of this shot to the given value. This will be shown on the UI if progress
        is set to a value less than zero. If progress is > 0 then the progress bar will be shown
        on the UI instead. Progress and Status Message are cosmetic.
        
        For C++ implementations override `virtual void SetStatusMessage_Implementation() override`
        For Python/BP implementations override
        unreal.ufunction(override=True): def set_status_message(self, inStatus):
        
        Args:
            status (str): The status message you wish the executor to have.
        """
        ...
    def set_shot_override_preset_origin(self, preset: MoviePipelineShotConfig) -> None:
        r"""
        x.set_shot_override_preset_origin(preset) -> None
        Set Shot Override Preset Origin
        
        Args:
            preset (MoviePipelineShotConfig):
        """
        ...
    def set_shot_override_configuration(self, preset: MoviePipelineShotConfig) -> None:
        r"""
        x.set_shot_override_configuration(preset) -> None
        Set Shot Override Configuration
        
        Args:
            preset (MoviePipelineShotConfig):
        """
        ...
    def set_graph_preset(self, graph_preset: MovieGraphConfig, update_variable_assignments: bool = True) -> None:
        r"""
        x.set_graph_preset(graph_preset, update_variable_assignments=True) -> None
        Sets the graph-style preset that this job will use. Note that this will cause the graph to switch over to using
        graph-style configuration if it is not already using it.
        
        Args:
            graph_preset (MovieGraphConfig): The graph preset to assign to the shot.
            update_variable_assignments (bool): Set to false if variable assignments should NOT be automatically updated to reflect the graph preset being used. This is normally not what you want and should be used with caution.
        """
        ...
    def is_using_graph_configuration(self) -> bool:
        r"""
        x.is_using_graph_configuration() -> bool
        Returns true if this job is using graph-style configuration, else false.
        
        Returns:
            bool:
        """
        ...
    def get_status_progress(self) -> float:
        r"""
        x.get_status_progress() -> float
        Get the current progress as last set by SetStatusProgress. 0 by default.
        
        For C++ implementations override `virtual float GetStatusProgress_Implementation() override`
        For Python/BP implementations override
        unreal.ufunction(override=True): def get_status_progress(self): return ?
        
        Returns:
            float:
        """
        ...
    def get_status_message(self) -> str:
        r"""
        x.get_status_message() -> str
        Get the current status message for this shot. May be an empty string.
        
        For C++ implementations override `virtual FString GetStatusMessage_Implementation() override`
        For Python/BP implementations override
        unreal.ufunction(override=True): def get_status_message(self): return ?
        
        Returns:
            str:
        """
        ...
    def get_shot_override_preset_origin(self) -> MoviePipelineShotConfig:
        r"""
        x.get_shot_override_preset_origin() -> MoviePipelineShotConfig
        Get Shot Override Preset Origin
        
        Returns:
            MoviePipelineShotConfig:
        """
        ...
    def get_shot_override_configuration(self) -> MoviePipelineShotConfig:
        r"""
        x.get_shot_override_configuration() -> MoviePipelineShotConfig
        Get Shot Override Configuration
        
        Returns:
            MoviePipelineShotConfig:
        """
        ...
    def get_or_create_variable_overrides(self, graph: MovieGraphConfig, is_for_primary_overrides: bool = False) -> MovieJobVariableAssignmentContainer:
        r"""
        x.get_or_create_variable_overrides(graph, is_for_primary_overrides=False) -> MovieJobVariableAssignmentContainer
        This method will return you the object which contains variable overrides for either the Job's Primary or the Shot's GraphPreset. UMoviePipelineExecutorShot
        has two separate sets of overrides. You can use the shot to override a variable on the Primary Graph (ie: the one assigned to the whole job),
        but a graph can also have an entirely separate UMovieGraph config asset to run (though at runtime some variables will only be read from the
        Primary Graph, ie: Custom Frame Range due to it applying to the entire sequence).
        
        If you specify true for bIsForPrimaryOverrides it returns an object that allows this shot to override a variable that comes from the primary
        graph. If you return false, then it returns an object that allows overriding a variable for this shot's override config (see: GetGraphPreset).
        See UMoviePipelineExecutorJob's version of this functoin for more details.
        
        Args:
            graph (MovieGraphConfig): The graph asset to return the config for. If this shot has its own Graph Preset override, you should return GetGraphPreset() or one of it's sub-graph pointers. If this shot is just trying to override the Primary Graph from the parent UMoviePipelineExecutorJob then you should return a pointer to the Job's GetGraphPreset() (or one of it's sub-graphs). Each graph/sub-graph gets its own set of overrides since sub-graphs can have different variables than the parents, so you have to provide the pointer to the one you want to override variables for.
            is_for_primary_overrides (bool): 
        
        Returns:
            MovieJobVariableAssignmentContainer: A container object which holds a copy of the variables for the specified Graph Asset that can be used to override their values on jobs without actually editing the default asset.
        """
        ...
    def get_graph_preset(self) -> MovieGraphConfig:
        r"""
        x.get_graph_preset() -> MovieGraphConfig
        Gets the graph-style preset that this job is using. If the job is not using a graph-style preset, returns nullptr.
        
        Returns:
            MovieGraphConfig:
        """
        ...
    def get_camera_name(self, camera_index: int) -> str:
        r"""
        x.get_camera_name(camera_index) -> str
        Get Camera Name
        
        Args:
            camera_index (int32): 
        
        Returns:
            str:
        """
        ...
    def allocate_new_shot_override_config(self, config_type: Class = None) -> MoviePipelineShotConfig:
        r"""
        x.allocate_new_shot_override_config(config_type=None) -> MoviePipelineShotConfig
        Allocate New Shot Override Config
        
        Args:
            config_type (type(Class)): 
        
        Returns:
            MoviePipelineShotConfig:
        """
        ...

class MoviePipelineQueue(Object):
    r"""
    A queue is a list of jobs that have been executed, are executing and are waiting to be executed. These can be saved
    to specific assets to allow
    
    **C++ Source:**
    
    - **Plugin**: MovieRenderPipeline
    - **Module**: MovieRenderPipelineCore
    - **File**: MoviePipelineQueue.h
    
    """
    def set_queue_origin(self, config: MoviePipelineQueue) -> None:
        r"""
        x.set_queue_origin(config) -> None
        Sets the queue that this queue originated from (if any). The origin should only be set for transient queues.
        
        Args:
            config (MoviePipelineQueue):
        """
        ...
    def set_job_index(self, job: MoviePipelineExecutorJob, index: int) -> None:
        r"""
        x.set_job_index(job, index) -> None
        Set the index of the given job
        
        Args:
            job (MoviePipelineExecutorJob): 
            index (int32):
        """
        ...
    def set_is_dirty(self, new_dirty_state: bool) -> None:
        r"""
        x.set_is_dirty(new_dirty_state) -> None
        Sets the dirty state of this queue. Generally the queue will correctly track the dirty state; however, there are
        situations where a queue may need its dirty state reset (eg, it may be appropriate to reset the dirty state after
        a call to CopyFrom(), depending on the use case).
        
        Args:
            new_dirty_state (bool):
        """
        ...
    def is_dirty(self) -> bool:
        r"""
        x.is_dirty() -> bool
        Gets the dirty state of this queue. Note that dirty state is only tracked when the editor is active.
        
        Returns:
            bool:
        """
        ...
    def get_queue_origin(self) -> MoviePipelineQueue:
        r"""
        x.get_queue_origin() -> MoviePipelineQueue
        Gets the queue that this queue was originally based on (if any). The origin will only be set on transient
        queues; the origin will be nullptr for non-transient queues because the origin will be this object.
        
        Returns:
            MoviePipelineQueue:
        """
        ...
    def get_jobs(self) -> Array[MoviePipelineExecutorJob]:
        r"""
        x.get_jobs() -> Array[MoviePipelineExecutorJob]
        Get all of the Jobs contained in this Queue.
        
        Returns:
            Array[MoviePipelineExecutorJob]: The jobs contained by this queue.
        """
        ...
    def duplicate_job(self, job: MoviePipelineExecutorJob) -> MoviePipelineExecutorJob:
        r"""
        x.duplicate_job(job) -> MoviePipelineExecutorJob
        Duplicate the specific job and return the duplicate. Configurations are duplicated and not shared.
        
        Args:
            job (MoviePipelineExecutorJob): The job to look for to duplicate.
        
        Returns:
            MoviePipelineExecutorJob: The duplicated instance or nullptr if a duplicate could not be made.
        """
        ...
    def delete_job(self, job: MoviePipelineExecutorJob) -> None:
        r"""
        x.delete_job(job) -> None
        Deletes the specified job from the Queue.
        
        Args:
            job (MoviePipelineExecutorJob): The job to look for and delete.
        """
        ...
    def delete_all_jobs(self) -> None:
        r"""
        x.delete_all_jobs() -> None
        Delete all jobs from the Queue.
        """
        ...
    def copy_from(self, queue: MoviePipelineQueue) -> MoviePipelineQueue:
        r"""
        x.copy_from(queue) -> MoviePipelineQueue
        Replace the contents of this queue with a copy of the contents from another queue.
        Returns a pointer to this queue if the copy was successful, else nullptr.
        
        Args:
            queue (MoviePipelineQueue): 
        
        Returns:
            MoviePipelineQueue:
        """
        ...
    def allocate_new_job(self, job_type: Class = None) -> MoviePipelineExecutorJob:
        r"""
        x.allocate_new_job(job_type=None) -> MoviePipelineExecutorJob
        Allocates a new Job in this Queue. The Queue owns the jobs for memory management purposes,
        and this will handle that for you.
        
        Args:
            job_type (type(Class)): Specify the specific Job type that should be created. Custom Executors can use custom Job types to allow the user to provide more information.
        
        Returns:
            MoviePipelineExecutorJob: The created Executor job instance.
        """
        ...

        class MoviePipelineInProcessExecutorSettings(DeveloperSettings):
    r"""
    This is the implementation responsible for executing the rendering of
    multiple movie pipelines after being launched via the command line.
    
    **C++ Source:**
    
    - **Plugin**: MovieRenderPipeline
    - **Module**: MovieRenderPipelineCore
    - **File**: MoviePipelineInProcessExecutorSettings.h
    
    **Editor Properties:** (see get_editor_property/set_editor_property)
    
    - ``additional_command_line_arguments`` (str):  [Read-Write] A list of additional command line arguments to be appended to the new process startup. In the form of "-foo -bar=baz". Can be useful if your game requires certain arguments to start such as disabling log-in screens.
    - ``close_editor`` (bool):  [Read-Write] If enabled the editor will close itself when a new process is started. This can be used to gain some performance.
    - ``inherited_command_line_arguments`` (str):  [Read-Only] A list of command line arguments which are inherited from the currently running Editor instance that will be automatically appended to the new process.
    - ``initial_delay_frame_count`` (int32):  [Read-Write] How long should we wait after being initialized to start doing any work? This can be used
      to work around situations where the game is not fully loaded by the time the pipeline
      is automatically started and it is important that the game is fully loaded before we do
      any work (such as evaluating frames for warm-up).
    """
    @property
    def close_editor(self) -> bool:
        r"""
        (bool):  [Read-Write] If enabled the editor will close itself when a new process is started. This can be used to gain some performance.
        """
        ...
    @close_editor.setter
    def close_editor(self, value: bool) -> None:
        ...
    @property
    def additional_command_line_arguments(self) -> str:
        r"""
        (str):  [Read-Write] A list of additional command line arguments to be appended to the new process startup. In the form of "-foo -bar=baz". Can be useful if your game requires certain arguments to start such as disabling log-in screens.
        """
        ...
    @additional_command_line_arguments.setter
    def additional_command_line_arguments(self, value: str) -> None:
        ...
    @property
    def inherited_command_line_arguments(self) -> str:
        r"""
        (str):  [Read-Only] A list of command line arguments which are inherited from the currently running Editor instance that will be automatically appended to the new process.
        """
        ...
    @property
    def initial_delay_frame_count(self) -> int:
        r"""
        (int32):  [Read-Write] How long should we wait after being initialized to start doing any work? This can be used
        to work around situations where the game is not fully loaded by the time the pipeline
        is automatically started and it is important that the game is fully loaded before we do
        any work (such as evaluating frames for warm-up).
        """
        ...
    @initial_delay_frame_count.setter
    def initial_delay_frame_count(self, value: int) -> None:
        ...

class MoviePipelineLinearExecutorBase(MoviePipelineExecutorBase):
    r"""
    This is an abstract base class designed for executing an array of movie pipelines in linear
    fashion. It is generally the case that you only want to execute one at a time on a local machine
    and a different executor approach should be taken for a render farm that distributes out jobs
    to many different machines.
    
    **C++ Source:**
    
    - **Plugin**: MovieRenderPipeline
    - **Module**: MovieRenderPipelineCore
    - **File**: MoviePipelineLinearExecutor.h
    
    **Editor Properties:** (see get_editor_property/set_editor_property)
    
    - ``debug_widget_class`` (type(Class)):  [Read-Write]
    - ``http_response_recieved_delegate`` (MoviePipelineHttpResponseRecieved):  [Read-Write] If an HTTP Request has been made and a response returned, the returned response will be broadcast on this event.
    - ``on_executor_errored_delegate`` (OnMoviePipelineExecutorErrored):  [Read-Write] Called when an individual job reports a warning/error. Jobs are considered fatal
      if the severity was bad enough to abort the job (missing sequence, write failure, etc.)
      
      Exposed for Blueprints/Python. Called at the same time as the native one.
    - ``on_executor_finished_delegate`` (OnMoviePipelineExecutorFinished):  [Read-Write] Called when the Executor has finished all jobs. Reports success if no jobs
      had fatal errors. Subscribe to the error delegate for more information about
      any errors.
      
      Exposed for Blueprints/Python. Called at the same time as the native one.
    - ``socket_message_recieved_delegate`` (MoviePipelineSocketMessageRecieved):  [Read-Write] If this executor has been configured to connect to a socket, this event will be called each time the socket recieves something.
      The message response expected from the server should have a 4 byte (int32) size prefix for the string to specify how much data
      we should expect. This delegate will not get invoked until we recieve that many bytes from the socket connection to avoid broadcasting
      partial messages.
    - ``target_pipeline_class`` (type(Class)):  [Read-Write] Which Pipeline Class should be created by this Executor. May be null.
    - ``user_data`` (str):  [Read-Write] Arbitrary data that can be associated with the executor. Not used by default implementations, nor read.
      This can be used to attach third party metadata such as job ids from remote farms.
    """
    ...

    class MoviePipelinePythonHostExecutor(MoviePipelineExecutorBase):
    r"""
    This is a dummy executor that is designed to host a executor implemented in
    python. Python defined UClasses are not available when the executor is initialized
    and not all callbacks are available in Python. By inheriting from this in Python
    and overriding which UClass to latently spawn, this class can just forward certain
    events onto Python (by overriding the relevant function).
    
    **C++ Source:**
    
    - **Plugin**: MovieRenderPipeline
    - **Module**: MovieRenderPipelineCore
    - **File**: MoviePipelinePythonHostExecutor.h
    
    **Editor Properties:** (see get_editor_property/set_editor_property)
    
    - ``debug_widget_class`` (type(Class)):  [Read-Write]
    - ``executor_class`` (type(Class)):  [Read-Write] You should override this class type on the CDO of the object with your Python type when Python is initialized.
    - ``http_response_recieved_delegate`` (MoviePipelineHttpResponseRecieved):  [Read-Write] If an HTTP Request has been made and a response returned, the returned response will be broadcast on this event.
    - ``on_executor_errored_delegate`` (OnMoviePipelineExecutorErrored):  [Read-Write] Called when an individual job reports a warning/error. Jobs are considered fatal
      if the severity was bad enough to abort the job (missing sequence, write failure, etc.)
      
      Exposed for Blueprints/Python. Called at the same time as the native one.
    - ``on_executor_finished_delegate`` (OnMoviePipelineExecutorFinished):  [Read-Write] Called when the Executor has finished all jobs. Reports success if no jobs
      had fatal errors. Subscribe to the error delegate for more information about
      any errors.
      
      Exposed for Blueprints/Python. Called at the same time as the native one.
    - ``pipeline_queue`` (MoviePipelineQueue):  [Read-Write]
    - ``socket_message_recieved_delegate`` (MoviePipelineSocketMessageRecieved):  [Read-Write] If this executor has been configured to connect to a socket, this event will be called each time the socket recieves something.
      The message response expected from the server should have a 4 byte (int32) size prefix for the string to specify how much data
      we should expect. This delegate will not get invoked until we recieve that many bytes from the socket connection to avoid broadcasting
      partial messages.
    - ``target_pipeline_class`` (type(Class)):  [Read-Write] Which Pipeline Class should be created by this Executor. May be null.
    - ``user_data`` (str):  [Read-Write] Arbitrary data that can be associated with the executor. Not used by default implementations, nor read.
      This can be used to attach third party metadata such as job ids from remote farms.
    """
    @property
    def executor_class(self) -> Class:
        r"""
        (type(Class)):  [Read-Write] You should override this class type on the CDO of the object with your Python type when Python is initialized.
        """
        ...
    @executor_class.setter
    def executor_class(self, value: Class) -> None:
        ...
    @property
    def pipeline_queue(self) -> MoviePipelineQueue:
        r"""
        (MoviePipelineQueue):  [Read-Write]
        """
        ...
    @pipeline_queue.setter
    def pipeline_queue(self, value: MoviePipelineQueue) -> None:
        ...
    def on_map_load(self, world: World) -> None:
        r"""
        x.on_map_load(world) -> None
        On Map Load
        
        Args:
            world (World):
        """
        ...
    def get_last_loaded_world(self) -> World:
        r"""
        x.get_last_loaded_world() -> World
        ~Python/Blueprint API
        
        Returns:
            World:
        """
        ...
    def execute_delayed(self, pipeline_queue: MoviePipelineQueue) -> None:
        r"""
        x.execute_delayed(pipeline_queue) -> None
        Python/Blueprint API
        
        Args:
            pipeline_queue (MoviePipelineQueue):
        """
        ...


class MoviePipelineNewProcessExecutor(MoviePipelineExecutorBase):
    r"""
    This is the implementation responsible for executing the rendering of
    multiple movie pipelines on the local machine in an external process.
    This simply handles launching and managing the external processes and
    acts as a proxy to them where possible. This internally uses the
    UMoviePipelineInProcessExecutor on the launched instances.
    
    **C++ Source:**
    
    - **Plugin**: MovieRenderPipeline
    - **Module**: MovieRenderPipelineEditor
    - **File**: MoviePipelineNewProcessExecutor.h
    
    **Editor Properties:** (see get_editor_property/set_editor_property)
    
    - ``debug_widget_class`` (type(Class)):  [Read-Write]
    - ``http_response_recieved_delegate`` (MoviePipelineHttpResponseRecieved):  [Read-Write] If an HTTP Request has been made and a response returned, the returned response will be broadcast on this event.
    - ``on_executor_errored_delegate`` (OnMoviePipelineExecutorErrored):  [Read-Write] Called when an individual job reports a warning/error. Jobs are considered fatal
      if the severity was bad enough to abort the job (missing sequence, write failure, etc.)
      
      Exposed for Blueprints/Python. Called at the same time as the native one.
    - ``on_executor_finished_delegate`` (OnMoviePipelineExecutorFinished):  [Read-Write] Called when the Executor has finished all jobs. Reports success if no jobs
      had fatal errors. Subscribe to the error delegate for more information about
      any errors.
      
      Exposed for Blueprints/Python. Called at the same time as the native one.
    - ``socket_message_recieved_delegate`` (MoviePipelineSocketMessageRecieved):  [Read-Write] If this executor has been configured to connect to a socket, this event will be called each time the socket recieves something.
      The message response expected from the server should have a 4 byte (int32) size prefix for the string to specify how much data
      we should expect. This delegate will not get invoked until we recieve that many bytes from the socket connection to avoid broadcasting
      partial messages.
    - ``target_pipeline_class`` (type(Class)):  [Read-Write] Which Pipeline Class should be created by this Executor. May be null.
    - ``user_data`` (str):  [Read-Write] Arbitrary data that can be associated with the executor. Not used by default implementations, nor read.
      This can be used to attach third party metadata such as job ids from remote farms.
    """
    ...

class MoviePipelinePIEExecutorSettings(DeveloperSettings):
    r"""
    This is the implementation responsible for executing the rendering of
    multiple movie pipelines within the editor using PIE.
    
    **C++ Source:**
    
    - **Plugin**: MovieRenderPipeline
    - **Module**: MovieRenderPipelineEditor
    - **File**: MoviePipelinePIEExecutorSettings.h
    
    **Editor Properties:** (see get_editor_property/set_editor_property)
    
    - ``initial_delay_frame_count`` (int32):  [Read-Write] How long should we wait after being initialized to start doing any work? This can be used
      to work around situations where the game is not fully loaded by the time the pipeline
      is automatically started and it is important that the game is fully loaded before we do
      any work (such as evaluating frames for warm-up).
    - ``resize_pie_window_to_output_resolution`` (bool):  [Read-Write] Should the PIE Window be created at the same resolution as the MRQ Output? By default we create the window at 720p for a nicer
      user experience, but this can be used to work around a widget scaling issue with UMG Widgets when using the UI Renderer
      setting. PIE is still limited by your monitor's resolution so you will need a monitor at least as big as your requested output
      for this to work (or can be combined with launching the editor with -ForceRes).
      
      Warning: Don't use this setting in combination with HighResTiling, as the main backbuffer will have to get resized to your final
      output resolution which will be too large.
    """
    @property
    def initial_delay_frame_count(self) -> int:
        r"""
        (int32):  [Read-Write] How long should we wait after being initialized to start doing any work? This can be used
        to work around situations where the game is not fully loaded by the time the pipeline
        is automatically started and it is important that the game is fully loaded before we do
        any work (such as evaluating frames for warm-up).
        """
        ...
    @initial_delay_frame_count.setter
    def initial_delay_frame_count(self, value: int) -> None:
        ...
    @property
    def resize_pie_window_to_output_resolution(self) -> bool:
        r"""
        (bool):  [Read-Write] Should the PIE Window be created at the same resolution as the MRQ Output? By default we create the window at 720p for a nicer
        user experience, but this can be used to work around a widget scaling issue with UMG Widgets when using the UI Renderer
        setting. PIE is still limited by your monitor's resolution so you will need a monitor at least as big as your requested output
        for this to work (or can be combined with launching the editor with -ForceRes).
        
        Warning: Don't use this setting in combination with HighResTiling, as the main backbuffer will have to get resized to your final
        output resolution which will be too large.
        """
        ...
    @resize_pie_window_to_output_resolution.setter
    def resize_pie_window_to_output_resolution(self, value: bool) -> None:
        ...

class MoviePipelineInProcessExecutor(MoviePipelineLinearExecutorBase):
    r"""
    This executor implementation can process an array of movie pipelines and
    run them inside the currently running process. This is intended for usage
    outside of the editor (ie. -game mode) as it will take over the currently
    running world/game instance instead of launching a new world instance like
    the editor only PIE.
    
    **C++ Source:**
    
    - **Plugin**: MovieRenderPipeline
    - **Module**: MovieRenderPipelineCore
    - **File**: MoviePipelineInProcessExecutor.h
    
    **Editor Properties:** (see get_editor_property/set_editor_property)
    
    - ``debug_widget_class`` (type(Class)):  [Read-Write]
    - ``http_response_recieved_delegate`` (MoviePipelineHttpResponseRecieved):  [Read-Write] If an HTTP Request has been made and a response returned, the returned response will be broadcast on this event.
    - ``on_executor_errored_delegate`` (OnMoviePipelineExecutorErrored):  [Read-Write] Called when an individual job reports a warning/error. Jobs are considered fatal
      if the severity was bad enough to abort the job (missing sequence, write failure, etc.)
      
      Exposed for Blueprints/Python. Called at the same time as the native one.
    - ``on_executor_finished_delegate`` (OnMoviePipelineExecutorFinished):  [Read-Write] Called when the Executor has finished all jobs. Reports success if no jobs
      had fatal errors. Subscribe to the error delegate for more information about
      any errors.
      
      Exposed for Blueprints/Python. Called at the same time as the native one.
    - ``socket_message_recieved_delegate`` (MoviePipelineSocketMessageRecieved):  [Read-Write] If this executor has been configured to connect to a socket, this event will be called each time the socket recieves something.
      The message response expected from the server should have a 4 byte (int32) size prefix for the string to specify how much data
      we should expect. This delegate will not get invoked until we recieve that many bytes from the socket connection to avoid broadcasting
      partial messages.
    - ``target_pipeline_class`` (type(Class)):  [Read-Write] Which Pipeline Class should be created by this Executor. May be null.
    - ``use_current_level`` (bool):  [Read-Write] Use current level instead of opening new level
    - ``user_data`` (str):  [Read-Write] Arbitrary data that can be associated with the executor. Not used by default implementations, nor read.
      This can be used to attach third party metadata such as job ids from remote farms.
    """
    @property
    def use_current_level(self) -> bool:
        r"""
        (bool):  [Read-Write] Use current level instead of opening new level
        """
        ...
    @use_current_level.setter
    def use_current_level(self, value: bool) -> None:
        ...

class MoviePipelinePIEExecutor(MoviePipelineLinearExecutorBase):
    r"""
    This is the implementation responsible for executing the rendering of
    multiple movie pipelines in the currently running Editor process. This
    involves launching a Play in Editor session for each Movie Pipeline to
    process.
    
    **C++ Source:**
    
    - **Plugin**: MovieRenderPipeline
    - **Module**: MovieRenderPipelineEditor
    - **File**: MoviePipelinePIEExecutor.h
    
    **Editor Properties:** (see get_editor_property/set_editor_property)
    
    - ``debug_widget_class`` (type(Class)):  [Read-Write]
    - ``http_response_recieved_delegate`` (MoviePipelineHttpResponseRecieved):  [Read-Write] If an HTTP Request has been made and a response returned, the returned response will be broadcast on this event.
    - ``on_executor_errored_delegate`` (OnMoviePipelineExecutorErrored):  [Read-Write] Called when an individual job reports a warning/error. Jobs are considered fatal
      if the severity was bad enough to abort the job (missing sequence, write failure, etc.)
      
      Exposed for Blueprints/Python. Called at the same time as the native one.
    - ``on_executor_finished_delegate`` (OnMoviePipelineExecutorFinished):  [Read-Write] Called when the Executor has finished all jobs. Reports success if no jobs
      had fatal errors. Subscribe to the error delegate for more information about
      any errors.
      
      Exposed for Blueprints/Python. Called at the same time as the native one.
    - ``on_individual_job_finished_delegate`` (OnMoviePipelineIndividualJobFinished):  [Read-Write]
    - ``on_individual_job_started_delegate`` (OnMoviePipelineIndividualJobStarted):  [Read-Write] Called right before this job is used to initialize a UMoviePipeline.
    - ``on_individual_job_work_finished_delegate`` (MoviePipelineWorkFinished):  [Read-Write] Called after each job is finished in the queue. Params struct contains an output of all files written.
    - ``on_individual_shot_work_finished_delegate`` (MoviePipelineWorkFinished):  [Read-Write] Called after each shot is finished for a particular render. Params struct contains an output of files written for this shot.
      Only called if the UMoviePipeline is set up correctly, requires a flag in the output setting to be set.
    - ``socket_message_recieved_delegate`` (MoviePipelineSocketMessageRecieved):  [Read-Write] If this executor has been configured to connect to a socket, this event will be called each time the socket recieves something.
      The message response expected from the server should have a 4 byte (int32) size prefix for the string to specify how much data
      we should expect. This delegate will not get invoked until we recieve that many bytes from the socket connection to avoid broadcasting
      partial messages.
    - ``target_pipeline_class`` (type(Class)):  [Read-Write] Which Pipeline Class should be created by this Executor. May be null.
    - ``user_data`` (str):  [Read-Write] Arbitrary data that can be associated with the executor. Not used by default implementations, nor read.
      This can be used to attach third party metadata such as job ids from remote farms.
    """
    @property
    def on_individual_job_finished_delegate(self) -> OnMoviePipelineIndividualJobFinished:
        r"""
        (OnMoviePipelineIndividualJobFinished):  [Read-Write]
        """
        ...
    @on_individual_job_finished_delegate.setter
    def on_individual_job_finished_delegate(self, value: OnMoviePipelineIndividualJobFinished) -> None:
        ...
    @property
    def on_individual_job_work_finished_delegate(self) -> MoviePipelineWorkFinished:
        r"""
        (MoviePipelineWorkFinished):  [Read-Write] Called after each job is finished in the queue. Params struct contains an output of all files written.
        """
        ...
    @on_individual_job_work_finished_delegate.setter
    def on_individual_job_work_finished_delegate(self, value: MoviePipelineWorkFinished) -> None:
        ...
    @property
    def on_individual_shot_work_finished_delegate(self) -> MoviePipelineWorkFinished:
        r"""
        (MoviePipelineWorkFinished):  [Read-Write] Called after each shot is finished for a particular render. Params struct contains an output of files written for this shot.
        Only called if the UMoviePipeline is set up correctly, requires a flag in the output setting to be set.
        """
        ...
    @on_individual_shot_work_finished_delegate.setter
    def on_individual_shot_work_finished_delegate(self, value: MoviePipelineWorkFinished) -> None:
        ...
    @property
    def on_individual_job_started_delegate(self) -> OnMoviePipelineIndividualJobStarted:
        r"""
        (OnMoviePipelineIndividualJobStarted):  [Read-Write] Called right before this job is used to initialize a UMoviePipeline.
        """
        ...
    @on_individual_job_started_delegate.setter
    def on_individual_job_started_delegate(self, value: OnMoviePipelineIndividualJobStarted) -> None:
        ...
    def set_is_rendering_offscreen(self, render_offscreen: bool) -> None:
        r"""
        x.set_is_rendering_offscreen(render_offscreen) -> None
        Should it render without any UI elements showing up (such as the rendering progress window)?
        
        Args:
            render_offscreen (bool):
        """
        ...
    def set_initialization_time(self, initialization_time: DateTime) -> None:
        r"""
        x.set_initialization_time(initialization_time) -> None
        Set Initialization Time
        
        Args:
            initialization_time (DateTime):
        """
        ...
    def set_allow_using_unsaved_levels(self, should_allow: bool) -> None:
        r"""
        x.set_allow_using_unsaved_levels(should_allow) -> None
        Set whether the executor should be allowed to use unsaved levels when rendering (by default, this is not permitted). There are some very
        specific circumstances where this may be OK. Do not set this unless you're certain it will not cause issues in your use case.
        
        Args:
            should_allow (bool):
        """
        ...
    def is_rendering_offscreen(self) -> bool:
        r"""
        x.is_rendering_offscreen() -> bool
        Will it render without any UI elements showing up (such as the rendering progress window)?
        
        Returns:
            bool:
        """
        ...

class MoviePipelineQueueEngineSubsystem(EngineSubsystem):
    r"""
    This subsystem is intended for use when rendering in a shipping game (but can also be used in PIE
    during development/testing). See UMoviePipelineQueueSubsystem for the Editor-only queue which is
    bound to the Movie Render Queue UI. To do simple renders at runtime, call AllocateJob(...)
    with the Level Sequence you want to render, then call FindOrAddSettingByClass on the job to add
    the settings (such as render pass, output type, and output directory) that you want for the job.
    Finally, call RenderJob with the job you just configured. Register a delegate to OnRenderFinished
    to be notified when the render finished. You can optionally call SetConfiguration(...) before
    RenderJob to configure some advanced options.
    
    **C++ Source:**
    
    - **Plugin**: MovieRenderPipeline
    - **Module**: MovieRenderPipelineCore
    - **File**: MoviePipelineQueueEngineSubsystem.h
    
    **Editor Properties:** (see get_editor_property/set_editor_property)
    
    - ``on_render_finished`` (MoviePipelineWorkFinished):  [Read-Write] Assign a function to this delegate to get notified when each individual job is finished.
      
      THIS WILL ONLY BE CALLED IF USING THE RENDERJOB CONVENIENCE FUNCTION.
      
      Because there can only be one job in the queue when using RenderJob, this will be called when
      the render is complete, and the executor has been released. This allows you to queue up another
      job immediately as a result of the OnRenderFinished callback.
    """
    @property
    def on_render_finished(self) -> MoviePipelineWorkFinished:
        r"""
        (MoviePipelineWorkFinished):  [Read-Write] Assign a function to this delegate to get notified when each individual job is finished.
        
        THIS WILL ONLY BE CALLED IF USING THE RENDERJOB CONVENIENCE FUNCTION.
        
        Because there can only be one job in the queue when using RenderJob, this will be called when
        the render is complete, and the executor has been released. This allows you to queue up another
        job immediately as a result of the OnRenderFinished callback.
        """
        ...
    @on_render_finished.setter
    def on_render_finished(self, value: MoviePipelineWorkFinished) -> None:
        ...
    def set_configuration(self, progress_widget_class: Class = None, render_player_viewport: bool = False) -> None:
        r"""
        x.set_configuration(progress_widget_class=None, render_player_viewport=False) -> None
        Sets some advanced configuration options that are used only occasionally to have better control over integrating it into
        your game/application. This applies to both RenderQueueWithExecutor(Instance) and the simplified RenderJob API. This persists
        until you call it again with different settings, and needs to be called before the Render* functions.
        
        Args:
            progress_widget_class (type(Class)): 
            render_player_viewport (bool): If true, we will render the regular player viewport in addition to the off-screen MRQ render. This is significantly performance heavy (doubles render times) but can be useful in the event that you want to keep rendering the player viewport to better integrate the render into your own application.
        """
        ...
    def render_queue_with_executor_instance(self, executor: MoviePipelineExecutorBase) -> None:
        r"""
        x.render_queue_with_executor_instance(executor) -> None
        Starts processing the current queue with the supplied executor. This starts an async process which
        may or may not run in a separate process (or on separate machines), determined by the executor implementation.
        The executor should report progress for jobs depending on the implementation.
        
        Args:
            executor (MoviePipelineExecutorBase): Instance of a subclass of UMoviePipelineExecutorBase.
        """
        ...
    def render_queue_with_executor(self, executor_type: Class) -> MoviePipelineExecutorBase:
        r"""
        x.render_queue_with_executor(executor_type) -> MoviePipelineExecutorBase
        Starts processing the current queue with the supplied executor class. This starts an async process which
        may or may not run in a separate process (or on separate machines), determined by the executor implementation.
        The executor should report progress for jobs depending on the implementation.
        
        Args:
            executor_type (type(Class)): A subclass of UMoviePipelineExecutorBase. An instance of this class is created and started.
        
        Returns:
            MoviePipelineExecutorBase: A pointer to the instance of the class created. This instance will be kept alive by the Queue Subsystem until it has finished (or been canceled). Register for progress reports and various callbacks on this instance.
        """
        ...
    def render_job(self, job: MoviePipelineExecutorJob) -> None:
        r"""
        x.render_job(job) -> None
        Convenience function for rendering the specified job. Calling this will render the specified job (if it was
        allocated using AllocateJob) and then it will reset the queue once finished. If you need to render multiple
        jobs (in a queue) then you need to either implement the queue behavior yourself, or use
        GetQueue()->AllocateJob(...) instead and use the non-convenience functions.
        
        Calling this function will clear the queue (after completion).
        
        Using this function while IsRendering() returns true will result in an exception being thrown and no attempt
        being made to render the job.
        
        Args:
            job (MoviePipelineExecutorJob):
        """
        ...
    def is_rendering(self) -> bool:
        r"""
        x.is_rendering() -> bool
        Returns true if there is an active executor working on producing a movie. This could be actively rendering frames,
        or working on post processing (finalizing file writes, etc.). Use GetActiveExecutor() and query it directly for
        more information, progress updates, etc.
        
        Returns:
            bool:
        """
        ...
    def get_queue(self) -> MoviePipelineQueue:
        r"""
        x.get_queue() -> MoviePipelineQueue
        Returns the queue of Movie Pipelines that need to be rendered.
        
        Returns:
            MoviePipelineQueue:
        """
        ...
    def get_active_executor(self) -> MoviePipelineExecutorBase:
        r"""
        x.get_active_executor() -> MoviePipelineExecutorBase
        Returns the active executor (if there is one). This can be used to subscribe to events on an already in-progress render. May be null.
        
        Returns:
            MoviePipelineExecutorBase:
        """
        ...
    def allocate_job(self, sequence: LevelSequence) -> MoviePipelineExecutorJob:
        r"""
        x.allocate_job(sequence) -> MoviePipelineExecutorJob
        Convenience function for creating a UMoviePipelineExecutorJob out of the given Level Sequence asset. The
        newly created job will be initialized to render on the current level, and will not have any default settings
        added to it - instead you will need to call FindOrAddSettingByClass on the job's configuration to add
        settings such as render passes (UMoviePipelineDeferredPassBase), output types (UMoviePipelineImageSequenceOutput_PNG),
        and configure the output directory (UMoviePipelineOutputSetting). Once configuration is complete, register
        a delegate to OnRenderFinished and then call RenderJob.
        
        Calling this function will clear the internal queue, see RenderJob for more details.
        
        Using this function while IsRendering() returns true will result in an exception being thrown and no attempt
        being made to create the job.
        
        Args:
            sequence (LevelSequence): 
        
        Returns:
            MoviePipelineExecutorJob:
        """
        ...

class MoviePipelineQueueSubsystem(EditorSubsystem):
    r"""
    Movie Pipeline Queue Subsystem
    
    **C++ Source:**
    
    - **Plugin**: MovieRenderPipeline
    - **Module**: MovieRenderPipelineEditor
    - **File**: MoviePipelineQueueSubsystem.h
    
    """
    def render_queue_with_executor_instance(self, executor: MoviePipelineExecutorBase) -> None:
        r"""
        x.render_queue_with_executor_instance(executor) -> None
        Starts processing the current queue with the supplied executor. This starts an async process which
        may or may not run in a separate process (or on separate machines), determined by the executor implementation.
        The executor should report progress for jobs depending on the implementation.
        
        Args:
            executor (MoviePipelineExecutorBase): Instance of a subclass of UMoviePipelineExecutorBase.
        """
        ...
    def render_queue_with_executor(self, executor_type: Class) -> MoviePipelineExecutorBase:
        r"""
        x.render_queue_with_executor(executor_type) -> MoviePipelineExecutorBase
        Starts processing the current queue with the supplied executor class. This starts an async process which
        may or may not run in a separate process (or on separate machines), determined by the executor implementation.
        The executor should report progress for jobs depending on the implementation.
        
        Args:
            executor_type (type(Class)): A subclass of UMoviePipelineExecutorBase. An instance of this class is created and started.
        
        Returns:
            MoviePipelineExecutorBase: A pointer to the instance of the class created. This instance will be kept alive by the Queue Subsystem until it has finished (or been canceled). Register for progress reports and various callbacks on this instance.
        """
        ...
    def render_queue_instance_with_executor_instance(self, queue: MoviePipelineQueue, executor: MoviePipelineExecutorBase) -> None:
        r"""
        x.render_queue_instance_with_executor_instance(queue, executor) -> None
        Like RenderQueueWithExecutorInstance(), but a specific queue instance is used rather than the current queue.
        
        Args:
            queue (MoviePipelineQueue): 
            executor (MoviePipelineExecutorBase):
        """
        ...
    def render_queue_instance_with_executor(self, queue: MoviePipelineQueue, executor_type: Class) -> MoviePipelineExecutorBase:
        r"""
        x.render_queue_instance_with_executor(queue, executor_type) -> MoviePipelineExecutorBase
        Like RenderQueueWithExecutor(), but a specific queue instance is used rather than the current queue.
        
        Args:
            queue (MoviePipelineQueue): 
            executor_type (type(Class)): 
        
        Returns:
            MoviePipelineExecutorBase:
        """
        ...
    def load_queue(self, queue_to_load: MoviePipelineQueue, prompt_on_replacing_dirty_queue: bool = True) -> bool:
        r"""
        x.load_queue(queue_to_load, prompt_on_replacing_dirty_queue=True) -> bool
        Loads a new queue by copying it into the queue subsystem's current transient queue (the one returned by GetQueue()).
        
        If bPromptOnReplacingDirtyQueue is true and the current queue has been modified since being loaded, a dialog will prompt the
        user if they want to discard their changes. If this dialog is rejected, or there was an error loading the queue, returns
        false, else returns true. Note that bPromptOnReplacingDirtyQueue is effectively ignored if the application is in
        "unattended" mode because the dialog is auto-accepted.
        
        Args:
            queue_to_load (MoviePipelineQueue): 
            prompt_on_replacing_dirty_queue (bool): 
        
        Returns:
            bool:
        """
        ...
    def is_rendering(self) -> bool:
        r"""
        x.is_rendering() -> bool
        Returns true if there is an active executor working on producing a movie. This could be actively rendering frames,
        or working on post processing (finalizing file writes, etc.). Use GetActiveExecutor() and query it directly for
        more information, progress updates, etc.
        
        Returns:
            bool:
        """
        ...
    def is_queue_dirty(self) -> bool:
        r"""
        x.is_queue_dirty() -> bool
        Returns true if the current queue has been modified since it was loaded, else returns false. Note that the empty
        queue that is in use upon the initial load of MRQ is not considered dirty.
        
        Returns:
            bool:
        """
        ...
    def get_queue(self) -> MoviePipelineQueue:
        r"""
        x.get_queue() -> MoviePipelineQueue
        Returns the queue of Movie Pipelines that need to be rendered.
        
        Returns:
            MoviePipelineQueue:
        """
        ...
    def get_active_executor(self) -> MoviePipelineExecutorBase:
        r"""
        x.get_active_executor() -> MoviePipelineExecutorBase
        Returns the active executor (if there is one). This can be used to subscribe to events on an already in-progress render. May be null.
        
        Returns:
            MoviePipelineExecutorBase:
        """
        ...

