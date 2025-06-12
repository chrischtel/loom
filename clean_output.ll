; ModuleID = 'MyLoomModule'
source_filename = "MyLoomModule"

define i32 @main() {
entry:
  %x = alloca i8, align 1
  store i8 42, ptr %x, align 1
  ret i32 0
}
