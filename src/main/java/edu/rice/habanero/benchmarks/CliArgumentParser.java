package edu.rice.habanero.benchmarks;

import java.io.IOException;
import java.util.Map;
import java.util.HashMap;

/**
 * @author <a href="http://shams.web.rice.edu/">Shams Imam</a> (shams@rice.edu)
 */
public class CliArgumentParser {

    private Map<String, String> cliArgs = new HashMap<>();

    public CliArgumentParser(final String[] args) {
        int i = 0;
        while (i < args.length) {
            String value = new String();
            String key = args[i];
            if (key.charAt(0) == '-') {
                //it is an cli argument
                int idx = key.indexOf('=');
                if (idx != -1) {
                    // is a value assigned with '='
                    value = key.substring(idx + 1);
                    key = key.substring(0, idx);
                } else if ((i + 1) < args.length && args[i + 1].charAt(0) != '-') {
                    // has next argument und is it a value
                    value = args[i + 1];
                    i++;
                } else {
                    // value must be a boolean 
                    value = "true";
                }
          } else {
              System.out.println("Command line argument expected but got value: "  + key);
              System.exit(1);
          }
          cliArgs.put(key, value);
          i++;
        }
    }
  
    public int getValue(String key, int default_value) {
        String res = cliArgs.get(key);
        if (res == null) {
            return default_value;
        } else {
            return Integer.parseInt(res);
        }
    }

    public int getValue(String[] key, int default_value) {
        String res;
        for (int i = 0; i < key.length; ++i) {
          res = cliArgs.get(key[i]);
          if (res != null) {
              return Integer.parseInt(res); 
          }
        }
        return default_value;
    }

    public long getValue(String key, long default_value) {
        String res = cliArgs.get(key);
        if (res == null) {
            return default_value;
        } else {
            return Long.parseLong(res);
        }
    }

    public long getValue(String[] key, long default_value) {
        String res;
        for (int i = 0; i < key.length; ++i) {
          res = cliArgs.get(key[i]);
          if (res != null) {
              return Long.parseLong(res); 
          }
        }
        return default_value;
    }

    public double getValue(String key, double default_value) {
        String res = cliArgs.get(key);
        if (res == null) {
            return default_value;
        } else {
            return Double.parseDouble(res);
        }
    }

    public double getValue(String[] key, double default_value) {
        String res;
        for (int i = 0; i < key.length; ++i) {
          res = cliArgs.get(key[i]);
          if (res != null) {
              return Double.parseDouble(res); 
          }
        }
        return default_value;
    }

    public boolean getValue(String key, boolean default_value) {
        String res = cliArgs.get(key);
        if (res == null) {
            return default_value;
        } else {
            return Boolean.parseBoolean(res);
        }
    }

    public boolean getValue(String[] key, boolean default_value) {
        String res;
        for (int i = 0; i < key.length; ++i) {
          res = cliArgs.get(key[i]);
          if (res != null) {
              return Boolean.parseBoolean(res); 
          }
        }
        return default_value;
    }
}
