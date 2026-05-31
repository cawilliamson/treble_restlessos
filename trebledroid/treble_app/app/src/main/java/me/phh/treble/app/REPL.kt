package me.phh.treble.app

import android.annotation.SuppressLint
import android.app.Service
import android.content.Intent
import android.os.IBinder
import android.util.Log
import java.lang.reflect.Method
import java.lang.reflect.Modifier
import java.net.InetAddress
import java.net.ServerSocket
import java.util.*
import kotlin.collections.HashSet
import kotlin.concurrent.thread

class REPL : Service() {
    override fun onBind(intent: Intent?): IBinder? {
        return null
    }

    fun getAllMethods(c: Class<*>?): List<Method> {
        val allMethods = HashSet<Method>()
        var cl = c

        while(cl != null && cl != Object::class.java) {
            allMethods.addAll(cl.declaredMethods)
            cl = cl.superclass as Class<*>
        }
        return allMethods.toList()
    }

    @SuppressLint("UseValueOf")
    override fun onCreate() {
        super.onCreate()

        thread {
            val serverSocket = ServerSocket(8800, 1, InetAddress.getByName("localhost"))

            while(true) {
                val socket = serverSocket.accept()

                val input = socket.getInputStream().bufferedReader()
                val output = socket.getOutputStream().bufferedWriter()

                val stack = Stack<Any?>()

                while (true) {
                    output.write("> ")
                    output.flush()
                    val str = (input.readLine() ?: break).split(" ")

                    Log.d("PHH-REPL", "Parsing ${str.joinToString(";")}")

                    output.write("\r\n")
                    output.write("\r\n")

                    for (word in str) {
                        try {
                            if (word == ".") {
                                val o = stack.last()
                                if (o == null) {
                                    output.write("(null)")
                                    continue
                                }
                                output.write("${(o as Object).getClass().canonicalName}: ${o.toString()}\r\n")
                                val arrayLength = try {
                                    java.lang.reflect.Array.getLength(o)
                                } catch (t: Throwable) {
                                    -1
                                }
                                if (arrayLength >= 0) {
                                    output.write("[$arrayLength]")
                                    for (i in 0 until arrayLength) {
                                        output.write(" [$i] => ${java.lang.reflect.Array.get(o, i)}\r\n")
                                    }
                                }
                            } else if (word == "+") {
                                val o = stack.last()
                                stack.push(o)
                            } else if (word == "context") {
                                stack.push(this as Object)
                            } else if (word.startsWith("\"")) {
                                stack.push(word.substring(1, word.length) as Object)
                            } else if (word.startsWith((":"))) {
                                val className = word.substring(1, word.length )
                                stack.push(Class.forName(className) as Object)
                            } else if(word.startsWith(".new.")) {
                                val o = stack.pop()
                                val args = word.split(".")
                                val ctorI = Integer.parseInt(args[2])

                                val ctor = (o as Class<*>).declaredConstructors[ctorI]
                                val params = (0 until ctor.parameterCount).map { stack.pop() }.toTypedArray()
                                stack.push(ctor.newInstance( *params ))
                            } else if (word.startsWith(".")) {
                                val args = word.split(".")
                                val methodName = args[1]
                                val o = stack.pop()
                                val obj = if(o is Class<*>) o else (o as Object).getClass()

                                val methods = getAllMethods(obj).filter { it.name == methodName }

                                val method = if(methods.size == 1) methods[0] else if(args.size>=3) methods[args[2].toInt()] else null

                                if (method != null) {
                                    val m = method
                                    if ((m.modifiers and Modifier.STATIC) != 0) {
                                        val params = (0 until m.parameterCount).map { stack.pop() }.toTypedArray()
                                        stack.push(m.invoke(null, *params) as Object?)
                                    } else {
                                        val params = (0 until m.parameterCount).map { stack.pop() }.toTypedArray()
                                        stack.push(m.invoke(o, *params) as Object?)
                                    }
                                } else {
                                    stack.push(o)
                                    output.write("Couldn't find one matching function $methodName")
                                    output.write("\r\n")
                                    for (m in methods) {
                                        output.write("A:")
                                        output.write("\r\n")
                                        for (p in m.parameters) {
                                            output.write("- ${p.type.canonicalName} ${p.name}")
                                            output.write("\r\n")
                                        }
                                    }
                                }
                            } else if(word == "list") {
                                val o = stack.last()
                                val obj = if (o is Class<*>) o else (o as Object).getClass()
                                var methods = getAllMethods(obj).sortedBy { it.name }
                                if (o is Class<*>) {
                                    methods = methods.filter { (it.modifiers and Modifier.STATIC) != 0 }
                                }
                                output.write("Got methods:\r\n")
                                for (m in methods) {
                                    val params = m.parameters.map { i -> "${i.type} ${i.name}" }.joinToString(", ")
                                    output.write("- ${m.name}($params) => ${m.returnType.name} \r\n")
                                }

                                if (o is Class<*>) {
                                    val constructors = o.declaredConstructors
                                    output.write("Got constructors:\r\n")
                                    for (c in constructors) {
                                        val params = c.parameters.map { i -> "${i.type} ${i.name}" }.joinToString(", ")
                                        output.write("- ($params) \r\n")
                                    }
                                }
                            } else if(word.matches(Regex("^[0-9]+$"))) {
                                val v = Integer(Integer.parseInt(word))
                                stack.push(v as Object)
                            } else if(word.matches(Regex("^\\[[0-9]+$"))) {
                                stack.push(
                                        java.lang.reflect.Array.get(
                                                stack.pop(),
                                                word.substring(1, word.length).toInt()
                                        ))
                            } else if(word == "exchange" || word == "X") {
                                val a = stack.pop()
                                val b = stack.pop()
                                stack.push(a)
                                stack.push(b)
                            } else if(word == "drop" || word == "D") {
                                stack.pop()
                            } else if(word == "show") {
                                output.write("Content of the stack:\r\n")
                                for(i in stack) {
                                    if(i == null) {
                                        output.write("- (null)\r\n")
                                    } else {
                                        output.write("- ${(i as Object).getClass().canonicalName} $i\r\n")
                                    }
                                }
                            } else if(word == "null") {
                                stack.push(null)
                            }
                        } catch (t: Throwable) {
                            Log.d("PHH-REPL", "REPL crashed", t)
                            output.write("!!! $t\r\n")
                        }
                    }
                    output.newLine()
                    output.flush()
                }

            }
        }
    }
}