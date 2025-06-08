#include <lldb/API/LLDB.h>
#include <import>

extern "C" { path path_with_cstr(path, cstr); }

object dbg_poll(dbg debug) {
    lldb::SBEvent event;
    while (debug->active) {
        usleep(1000);
        if (!debug->running)
            continue;
        if (!debug->lldb_listener.WaitForEvent(1000, event))
            continue;
        if (!lldb::SBProcess::EventIsProcessEvent(event))
            continue;
    
        lldb::StateType   state      = lldb::SBProcess::GetStateFromEvent(event);
        lldb::SBThread    thread     = debug->lldb_process.GetSelectedThread();
        lldb::StopReason  reason     = thread.GetStopReason();
        lldb::SBFrame     frame      = thread.GetSelectedFrame();
        if (!frame.IsValid())
            continue; // how do we check for a valid frame here
        
        lldb::SBLineEntry line_entry = frame.GetLineEntry();
        lldb::SBFileSpec  file_spec  = line_entry.GetFileSpec();
        u32 line   = line_entry.GetLine();
        u32 column = line_entry.GetColumn();
        char file_path[1024];
        file_spec.GetPath(file_path, sizeof(file_path));
        path source = path_with_cstr(new(path), file_path); // when this happens, file_spec is blank, reason == eStopReasonInvalid, state == eStateRunning

        if (state == lldb::eStateStopped) {
            debug->running = false;
            cursor cur = cursor(
                debug,  debug,
                source, source,
                line,   line,
                column, column);
            debug->on_break((object)cur);
            
        } else if (state == lldb::eStateCrashed) {
            debug->running = false;
            cursor cur = cursor(
                debug,  debug,
                source, source,
                line,   line,
                column, column);
            debug->on_crash((object)cur);

        } else if (state == lldb::eStateExited) {
            debug->running = false;
            i32    exit_code = debug->lldb_process.GetExitStatus();
            exited e = exited(
                debug,  debug,
                code,   exit_code);
            debug->on_exit((object)e);
        }
    }
    return null;
}

none dbg_init(dbg debug) {
    static bool dbg_init = false;
    if (!dbg_init) {
        dbg_init = true;
        lldb::SBDebugger::Initialize();
    }
    debug->lldb_debugger = lldb::SBDebugger::Create();
    debug->lldb_debugger.SetAsync(true);
    if (!debug->exceptions) {
        lldb::SBCommandInterpreter  interp = debug->lldb_debugger.GetCommandInterpreter();
        lldb::SBCommandReturnObject result;
        interp.HandleCommand("settings set target.exception-breakpoints.* false", result);
    }
    debug->lldb_listener = debug->lldb_debugger.GetListener();
    debug->lldb_target   = debug->lldb_debugger.CreateTarget(debug->location->chars);
    debug->running       = false;

    if (debug->lldb_target.IsValid()) {
        debug->active        = true;
        debug->poll          = async(work, a((object)debug), work_fn, (hook)dbg_poll);
        if (debug->auto_start)
            start(debug);
    }
}

none dbg_dealloc(dbg debug) {
    stop(debug);
    debug->active        = false;
}

none dbg_start(dbg debug) {
    lldb::SBError      error;
    lldb::SBLaunchInfo launch_info(null);

    debug->lldb_process = debug->lldb_target.Launch(launch_info, error);

    if (!error.Success()) {
        print("failed: %s", error.GetCString());
    } else {
        print("launched: pid=%i", (i32)debug->lldb_process.GetProcessID());
        debug->running = true;
    }
}

none dbg_stop(dbg debug) {
    if (debug->running) return;
    if (debug->lldb_process.IsValid())
        debug->lldb_process.Detach();
    debug->running = false;
}

none dbg_step_into(dbg debug) {
    lldb::SBThread thread = debug->lldb_process.GetSelectedThread();
    thread.StepInto();
}

none dbg_step_over(dbg debug) {
    lldb::SBThread thread = debug->lldb_process.GetSelectedThread();
    thread.StepOver();
}

none dbg_step_out(dbg debug) {
    lldb::SBThread thread = debug->lldb_process.GetSelectedThread();
    thread.StepOut();
}

none dbg_pause(dbg debug) {
    debug->lldb_process.Stop();
}

none dbg_cont(dbg debug) {
    debug->lldb_process.Continue();
}

array read_children(dbg debug, lldb::SBValue value) {
    array result = array(alloc, 32);

    uint32_t num_children = value.GetNumChildren();
    for (uint32_t i = 0; i < num_children; ++i) {
        lldb::SBValue child = value.GetChildAtIndex(i);
        string        name  = string((cstr)child.GetName());
        string        type  = string((cstr)child.GetTypeName());
        string        val   = string((cstr)child.GetValue());
        char          name_buf[256];
        if (!len(name)) {
            snprintf(name_buf, sizeof(name_buf), "[%u]", i);
            name = string(name_buf);
        }
        variable v = variable(
            debug,      debug,
            name,       name,
            type,       type,
            value,      val,
            children,   read_children(debug, child));

        push(result, (object)v);
    }

    return result;
}

array dbg_read_vars(dbg debug, array result, lldb::SBValueList vars) {
    for (uint32_t i = 0; i < vars.GetSize(); ++i) {
        lldb::SBValue value = vars.GetValueAtIndex(i);
        string name = string(value.GetName());
        string type = string(value.GetTypeName());
        string val  = string(value.GetValue());
        variable v  = variable(
            debug,      debug,
            name,       name,
            type,       type,
            value,      val,
            children,   read_children(debug, value)
        );
        push(result, (object)v);
    }
    return result;
}

array dbg_read_arguments(dbg debug) {
    array             result = array(alloc, 32);
    lldb::SBFrame     frame  = debug->lldb_process.GetSelectedThread().GetSelectedFrame();
    lldb::SBValueList args   = frame.GetVariables(
        true, false, false, false); 
    return dbg_read_vars(debug, result, args);
}

array dbg_read_locals   (dbg debug) {
    array             result = array(alloc, 32);
    lldb::SBFrame     frame  = debug->lldb_process.GetSelectedThread().GetSelectedFrame();
    lldb::SBValueList args   = frame.GetVariables(
        false, true, false, false); 
    return dbg_read_vars(debug, result, args);
}

array dbg_read_statics  (dbg debug) {
    array             result = array(alloc, 32);
    lldb::SBFrame     frame  = debug->lldb_process.GetSelectedThread().GetSelectedFrame();
    lldb::SBValueList args   = frame.GetVariables(
        false, false, true, false); 
    return dbg_read_vars(debug, result, args);
}

array dbg_read_globals  (dbg debug) {
    array             result      = array(alloc, 32);
    lldb::SBFrame     frame       = debug->lldb_process.GetSelectedThread().GetSelectedFrame();
    u32               num_modules = debug->lldb_target.GetNumModules();

    for (u32 i = 0; i < num_modules; ++i) {
        lldb::SBModule    module  = debug->lldb_target.GetModuleAtIndex(i);
        lldb::SBValueList globals = debug->lldb_target.FindGlobalVariables(
            null, // you can use full file path or wildcard
            1000  // max number of globals to return
        );
        dbg_read_vars(debug, result, globals);
    }
    return result;
}

array dbg_read_registers(dbg debug) {
    array             result = array(alloc, 32);
    lldb::SBThread    thread = debug->lldb_process.GetSelectedThread();
    lldb::SBFrame     frame  = thread.GetSelectedFrame();
    lldb::SBValueList regs   = frame.GetRegisters();
    dbg_read_vars(debug, result, regs);
    return result;
}

define_class(cursor,   A)
define_class(exited,   A)
define_class(variable, A)
define_class(dbg,      A)
