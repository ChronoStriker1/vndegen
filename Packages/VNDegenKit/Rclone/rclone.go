// VNDegen's build of rclone: the remote control API of rclone's library (librclone), with only the backends VNDegen
// uses, calls that can be cancelled, and one call of VNDegen's own, vndegen/replacefile.
package main

/*
#include <stdlib.h>
*/
import "C"

import (
	"bytes"
	"context"
	"encoding/base64"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"strings"
	"sync"
	"unsafe"

	"github.com/rclone/rclone/backend/local"
	"github.com/rclone/rclone/backend/smb"
	"github.com/rclone/rclone/fs"
	"github.com/rclone/rclone/fs/accounting"
	"github.com/rclone/rclone/fs/config"
	"github.com/rclone/rclone/fs/config/configfile"
	"github.com/rclone/rclone/fs/log"
	_ "github.com/rclone/rclone/fs/operations"
	"github.com/rclone/rclone/fs/rc"
	"github.com/rclone/rclone/fs/rc/jobs"
	_ "github.com/rclone/rclone/fs/sync"
)

// Calls by ID: VNDRcloneBegin adds one, VNDRcloneCall runs and removes it, and VNDRcloneCancel cancels it while it is
// here. A cancel that comes before or after has nothing to keep.
var (
	lock    sync.Mutex
	running = map[int64]context.CancelFunc{}
	waiting = map[int64]context.Context{}
)

//export VNDRcloneInitialize
func VNDRcloneInitialize() {
	// In memory only, so the user's own rclone.conf is never read or written.
	_ = config.SetConfigPath("")
	log.InitLogging()
	configfile.Install()
	accounting.Start(context.Background())
}

//export VNDRcloneBegin
func VNDRcloneBegin(id C.longlong) {
	ctx, cancel := context.WithCancel(context.Background())
	lock.Lock()
	defer lock.Unlock()
	running[int64(id)] = cancel
	waiting[int64(id)] = ctx
}

//export VNDRcloneCall
func VNDRcloneCall(id C.longlong, method, input *C.char, status *C.int) *C.char {
	lock.Lock()
	ctx, ok := waiting[int64(id)]
	delete(waiting, int64(id))
	lock.Unlock()
	if !ok {
		out, code := failure(C.GoString(method), rc.Params{}, errors.New("call was not begun"), http.StatusInternalServerError)
		*status = C.int(code)
		return C.CString(out)
	}
	defer func() {
		lock.Lock()
		cancel := running[int64(id)]
		delete(running, int64(id))
		lock.Unlock()
		cancel()
	}()
	out, code := call(ctx, C.GoString(method), C.GoString(input))
	*status = C.int(code)
	return C.CString(out)
}

//export VNDRcloneCancel
func VNDRcloneCancel(id C.longlong) {
	lock.Lock()
	defer lock.Unlock()
	if cancel, ok := running[int64(id)]; ok {
		cancel()
	}
}

//export VNDRcloneFree
func VNDRcloneFree(s *C.char) { C.free(unsafe.Pointer(s)) }

// call is librclone's RPC with a context the caller can cancel.
func call(ctx context.Context, method, input string) (output string, status int) {
	in := rc.Params{}
	defer func() {
		if r := recover(); r != nil {
			output, status = failure(method, in, fmt.Errorf("panic: %v", r), http.StatusInternalServerError)
		}
	}()
	if input != "" {
		if err := json.NewDecoder(strings.NewReader(input)).Decode(&in); err != nil {
			return failure(method, in, err, http.StatusBadRequest)
		}
	}
	fn := rc.Calls.Get(method)
	if fn == nil || fn.NeedsRequest || fn.NeedsResponse {
		return failure(method, in, fmt.Errorf("no method %q", method), http.StatusNotFound)
	}
	_, out, err := jobs.NewJob(ctx, fn.Fn, in)
	if err != nil {
		return failure(method, in, err, http.StatusInternalServerError)
	}
	if out == nil {
		out = rc.Params{}
	}
	var w strings.Builder
	if err := rc.WriteJSON(&w, out); err != nil {
		return failure(method, in, err, http.StatusInternalServerError)
	}
	return w.String(), http.StatusOK
}

func failure(method string, in rc.Params, err error, status int) (string, int) {
	out, status := rc.Error(method, in, err, status)
	var w strings.Builder
	_ = rc.WriteJSON(&w, out)
	return w.String(), status
}

func init() {
	rc.Add(rc.Call{
		Path:  "vndegen/replacefile",
		Fn:    replaceFile,
		Title: "Renames a file over another when the other still holds the expected bytes",
		Help: `Parameters: fs, srcRemote, dstRemote, and expected, the bytes dstRemote must hold (base64), or null when it
must not exist. Reads dstRemote whole, compares, and on a match renames srcRemote over it with the backend's own
rename, which replaces the file in one step. Returns replaced: false, and changes nothing, on a mismatch.`,
	})
}

// replaceFile is the last step of VNDegen's mapping.yaml commit. rclone's operations/movefile deletes an existing
// destination before it renames, which leaves a moment with no file at all, so this calls the backend's Move itself.
// Only backends whose Move replaces the destination in one step are allowed.
func replaceFile(ctx context.Context, in rc.Params) (rc.Params, error) {
	f, err := rc.GetFs(ctx, in)
	if err != nil {
		return nil, err
	}
	src, err := in.GetString("srcRemote")
	if err != nil {
		return nil, err
	}
	dst, err := in.GetString("dstRemote")
	if err != nil {
		return nil, err
	}
	var expected []byte
	if raw, ok := in["expected"]; ok && raw != nil {
		text, ok := raw.(string)
		if !ok {
			return nil, rc.NewErrParamInvalid(errors.New("expected must be base64 or null"))
		}
		if expected, err = base64.StdEncoding.DecodeString(text); err != nil {
			return nil, rc.NewErrParamInvalid(err)
		}
	}
	switch f.(type) {
	case *smb.Fs, *local.Fs:
	default:
		return nil, fmt.Errorf("%s can't replace a file in one step", f.Name())
	}
	move := f.Features().Move
	source, err := f.NewObject(ctx, src)
	if err != nil {
		return nil, err
	}
	current, err := readAll(ctx, f, dst)
	if err != nil {
		return nil, err
	}
	if (current == nil) != (expected == nil) || !bytes.Equal(current, expected) {
		return rc.Params{"replaced": false}, nil
	}
	// The commit point. A call cancelled before here never renames. The smb backend's own requests ignore the
	// context, so once Move starts, cancelling no longer stops it, and the caller treats the outcome as unknown.
	if err := ctx.Err(); err != nil {
		return nil, err
	}
	// The window: another device that writes dstRemote after the server sent the bytes read above and before the
	// rename completes is overwritten. On SMB that is six requests, one after another: the read that reaches the end
	// of the file, the read's CLOSE, the backend's parent folder check (CREATE and CLOSE), the rename's CREATE of the
	// source, and its SET_INFO, which replaces the file. A few milliseconds on a LAN.
	if _, err := move(ctx, source, dst); err != nil {
		return nil, err
	}
	return rc.Params{"replaced": true}, nil
}

// readAll returns the file's bytes, or nil when it does not exist.
func readAll(ctx context.Context, f fs.Fs, remote string) ([]byte, error) {
	obj, err := f.NewObject(ctx, remote)
	if errors.Is(err, fs.ErrorObjectNotFound) {
		return nil, nil
	}
	if err != nil {
		return nil, err
	}
	r, err := obj.Open(ctx)
	if err != nil {
		return nil, err
	}
	defer r.Close()
	var data []byte
	chunk := make([]byte, 64*1024)
	for {
		// Checked between reads, so a cancelled call stops at the next chunk. A single read already sent ends only
		// when its bytes are in or nothing arrives for the idle deadline.
		if err := ctx.Err(); err != nil {
			return nil, err
		}
		n, err := r.Read(chunk)
		data = append(data, chunk[:n]...)
		if err == io.EOF {
			break
		}
		if err != nil {
			return nil, err
		}
	}
	if data == nil {
		data = []byte{}
	}
	return data, nil
}

func main() {}
