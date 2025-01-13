use std::future::Future;
use std::future::IntoFuture;

use std::pin::Pin;

use std::task::Context;
use std::task::Poll;

use cxx::ExternType;

use crate::waker::deref_co_await_waker;

use crate::lazy_pin_init::LazyPinInit;

// =======================================================================================
// GuardedRustPromiseAwaiter

use crate::ffi::guarded_rust_promise_awaiter_drop_in_place;
use crate::ffi::guarded_rust_promise_awaiter_new_in_place;

#[path = "await.h.rs"]
mod await_h;
pub use await_h::GuardedRustPromiseAwaiter;

// Safety: KJ Promises are not associated with threads, but with event loops at construction time.
// Therefore, they can be polled from any thread, as long as that thread has the correct event loop
// active at the time of the call to `poll()`. If the correct event loop is not active,
// GuardedRustPromiseAwaiter's API will panic. (The Guarded- prefix refers to the C++ class template
// ExecutorGuarded, which enforces the correct event loop requirement.)
unsafe impl Send for GuardedRustPromiseAwaiter {}

impl Drop for GuardedRustPromiseAwaiter {
    fn drop(&mut self) {
        // Pin safety:
        // The pin crate suggests implementing drop traits for address-sensitive types with an inner
        // function which accepts a `Pin<&mut Type>` parameter, to help uphold pinning guarantees.
        // However, since our drop function is actually a C++ destructor to which we must pass a raw
        // pointer, there is no benefit in creating a Pin from `self`.
        //
        // https://doc.rust-lang.org/std/pin/index.html#implementing-drop-for-types-with-address-sensitive-states
        //
        // Pointer safety:
        // 1. Pointer to self is non-null, and obviously points to valid memory.
        // 2. We do not read or write to the OwnPromiseNode's memory, so there are no atomicity nor
        //    interleaved pointer/reference access concerns.
        //
        // https://doc.rust-lang.org/std/ptr/index.html#safety
        unsafe {
            guarded_rust_promise_awaiter_drop_in_place(PtrGuardedRustPromiseAwaiter(self));
        }
    }
}

// Safety: We have a static_assert in await.c++ which breaks if you change the size or alignment
// of the C++ definition of GuardedRustPromiseAwaiter, with instructions to regenerate the bindgen-
// generated type. I couldn't figure out how to static_assert on the actual generated Rust struct,
// though, so it's not perfect. Ideally we'd run bindgen in the build system.
//
// https://docs.rs/cxx/latest/cxx/trait.ExternType.html#integrating-with-bindgen-generated-types
unsafe impl ExternType for GuardedRustPromiseAwaiter {
    type Id = cxx::type_id!("workerd::rust::async::GuardedRustPromiseAwaiter");
    type Kind = cxx::kind::Opaque;
}

#[repr(transparent)]
pub struct PtrGuardedRustPromiseAwaiter(*mut GuardedRustPromiseAwaiter);

// Safety: Raw pointers are the same size in both languages.
unsafe impl ExternType for PtrGuardedRustPromiseAwaiter {
    type Id = cxx::type_id!("workerd::rust::async::PtrGuardedRustPromiseAwaiter");
    type Kind = cxx::kind::Trivial;
}

// =======================================================================================
// Await syntax for OwnPromiseNode

use crate::OwnPromiseNode;

impl IntoFuture for OwnPromiseNode {
    type Output = ();
    type IntoFuture = LazyRustPromiseAwaiter;

    fn into_future(self) -> Self::IntoFuture {
        LazyRustPromiseAwaiter::new(self)
    }
}

pub struct LazyRustPromiseAwaiter {
    node: Option<OwnPromiseNode>,
    awaiter: LazyPinInit<GuardedRustPromiseAwaiter>,
    // Safety: `rust_waker` must be declared after `awaiter`, because `awaiter` contains a reference
    // to `rust_waker`. This ensures `rust_waker` will be dropped after `awaiter`.
    rust_waker: RustWaker,
}

impl LazyRustPromiseAwaiter {
    fn new(node: OwnPromiseNode) -> Self {
        LazyRustPromiseAwaiter {
            node: Some(node),
            awaiter: LazyPinInit::uninit(),
            rust_waker: RustWaker::empty(),
        }
    }

    fn get_awaiter(mut self: Pin<&mut Self>) -> Pin<&mut GuardedRustPromiseAwaiter> {
        // On our first invocation, `node` will be Some, and `get_awaiter` will forward its
        // contents into GuardedRustPromiseAwaiter's constructor. On all subsequent invocations, `node`
        // will be None and the constructor will not run.
        let node = self.node.take();

        // Safety: `awaiter` stores `rust_waker_ptr` and uses it to call `wake()`. Note that
        // `awaiter` is `self.awaiter`, which lives before `self.rust_waker`. Since struct members
        // are dropped in declaration order, the `rust_waker_ptr` that `awaiter` stores will always
        // be valid during its lifetime.
        //
        // We pass a mutable pointer to C++. This is safe, because our use of the RustWaker inside
        // of `std::task::Waker` is synchronized by ensuring we only allow calls to `poll()` on the
        // thread with the Promise's event loop active.
        let rust_waker_ptr = &mut self.rust_waker as *mut RustWaker;

        // Safety:
        // 1. We do not implement Unpin for LazyRustPromiseAwaiter.
        // 2. Our Drop trait implementation does not move the awaiter value, nor do we use
        //    `repr(packed)` anywhere.
        // 3. The backing memory is inside our pinned Future, so we can be assured our Drop trait
        //    implementation will run before Rust re-uses the memory.
        //
        // https://doc.rust-lang.org/std/pin/index.html#choosing-pinning-to-be-structural-for-field
        let awaiter = unsafe { self.map_unchecked_mut(|s| &mut s.awaiter) };

        // Safety:
        // 1. We trust that LazyPinInit's implementation passed us a valid pointer to an
        //    uninitialized GuardedRustPromiseAwaiter.
        // 2. We do not read or write to the GuardedRustPromiseAwaiter's memory, so there are no atomicity
        //    nor interleaved pointer reference access concerns.
        //
        // https://doc.rust-lang.org/std/ptr/index.html#safety
        awaiter.get_or_init(move |ptr: *mut GuardedRustPromiseAwaiter| unsafe {
            guarded_rust_promise_awaiter_new_in_place(
                PtrGuardedRustPromiseAwaiter(ptr),
                rust_waker_ptr,
                node.expect("node should be Some in call to init()"),
            );
        })
    }
}

use crate::RustWaker;

impl Future for LazyRustPromiseAwaiter {
    type Output = ();
    fn poll(mut self: Pin<&mut Self>, cx: &mut Context) -> Poll<()> {
        let done = if let Some(kj_waker) = deref_co_await_waker(cx.waker()) {
            self.rust_waker.set_none();
            let awaiter = self.as_mut().get_awaiter();
            awaiter.poll_with_co_await_waker(kj_waker)
        } else {
            self.rust_waker.set(cx.waker());
            let awaiter = self.as_mut().get_awaiter();
            awaiter.poll()
        };

        if done {
            Poll::Ready(())
        } else {
            Poll::Pending
        }
    }
}
