﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Net;
using System.Net.Sockets;
using System.IO;
using System.Text.RegularExpressions;
using Herd;
using System.Threading;

namespace TESTHerdAgent
{
    class Program
    {
        static HerdAgentServiceWindows m_herdService;
       
        static void Main(string[] args)
        {
            System.Threading.Thread.Sleep(2000);

            m_herdService = new HerdAgentServiceWindows();

            m_herdService.DoStart();

            while (true)
            {
                Thread.Sleep(1000);
            }
           
        }
    }
}
